/*
 * Copyright IBM Corp. and others 2023
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.jep425;

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.time.Duration;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;
import java.util.stream.IntStream;

import org.openj9.test.util.VersionCheck;

/**
 * Test cases for JEP 425: Virtual Threads (Preview) Continuation execution
 * which verifies the basic cases including Continuation enter, yield, resume.
 */
@Test(groups = { "level.sanity" })
public class VirtualThreadTests {

	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {
			System.out.println("No natives for JNI tests");
		}
	}

	public static native boolean lockSupportPark();

	private void incrementalWait(Thread t) throws InterruptedException {
		/* Incrementally wait for 10000 ms. */
		for (int i = 0; i < 200; i++) {
			Thread.sleep(50);
			if (Thread.State.WAITING == t.getState()) {
				break;
			}
		}
	}

	@Test
	public void test_basicVirtualthread() {
		var wrapper = new Object() {
			boolean executed = false;
		};
		try {
			Thread t = Thread.ofVirtual().name("duke").unstarted(() -> {
				wrapper.executed = true;
			});

			t.start();
			t.join();

			AssertJUnit.assertTrue("Virtual Thread operation not executed", wrapper.executed);
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	@Test
	public void test_VirtualthreadYieldResume() {
		try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
			int numThreads = 6;
			int expectedThreadResult = 3;
			long estimatedThreadCompletionTime = 5L; /* seconds */
			int[] results = new int[numThreads];

			IntStream.range(0, numThreads).forEach(i -> {
				executor.submit(() -> {
					results[i] = 1;
					Thread.sleep(Duration.ofSeconds(1));
					results[i] += 1;
					Thread.sleep(Duration.ofSeconds(1));
					results[i] += 1;
					return i;
				});
			});

			/* Wait incrementally for the worst-case scenario where all virtual threads are
			 * executed sequentially. Exit the wait loop if the virtual threads finish early.
			 */
			for (int i = 0; i < numThreads; i++) {
				executor.awaitTermination(estimatedThreadCompletionTime, TimeUnit.SECONDS);
				boolean exit = true;
				for (int j = 0; j < numThreads; j++) {
					if (results[j] != expectedThreadResult) {
						exit = false;
					}
				}
				if (exit) {
					break;
				}
			}

			for (int i = 0; i < numThreads; i++) {
				AssertJUnit.assertTrue(
						"Virtual Thread " + i + ": incorrect result of " + results[i],
						(results[i] == expectedThreadResult));
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testSyncThreadReady = false;

	@Test
	public void test_synchronizedBlockFromVirtualthread() {
		try {
			Thread t = Thread.ofVirtual().name("synchronized").start(() -> {
				synchronized (VirtualThreadTests.class) {
					testSyncThreadReady = true;
					LockSupport.park();
				}
			});

			while (!testSyncThreadReady) {
				Thread.sleep(10);
			}
			/* Incrementally wait for 10000 ms to let the virtual thread park. */
			incrementalWait(t);
			Assert.assertEquals(t.getState(), Thread.State.WAITING);
			LockSupport.unpark(t);
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testJNIThreadReady = false;

	@Test
	public void test_jniFromVirtualthread() {
		try {
			Thread t = Thread.ofVirtual().name("native").start(() -> {
				testJNIThreadReady = true;
				lockSupportPark();
			});

			while (!testJNIThreadReady) {
				Thread.sleep(10);
			}

			/* Incrementally wait for 10000 ms to let the virtual thread park. */
			incrementalWait(t);
			Assert.assertEquals(t.getState(), Thread.State.WAITING);
			LockSupport.unpark(t);
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage(), e);
		}
	}

	private static volatile boolean testThread1Ready = false;

	@Test
	public void test_YieldedVirtualThreadGetStackTrace() {
		/* The expected frame count is based on test's callstack. */
		int expectedFrames = (VersionCheck.major() >= 24) ? 5 : 6;
		String expectedMethodName = "park";

		try {
			Thread t = Thread.ofVirtual().name("yielded-stackwalker").start(() -> {
				testThread1Ready = true;
				LockSupport.park();
			});

			while (!testThread1Ready) {
				Thread.sleep(10);
			}

			/* Incrementally wait for 10000 ms to let the virtual thread park. */
			incrementalWait(t);

			StackTraceElement[] ste = t.getStackTrace();

			/* If the stacktrace doesn't match the expected result, then print out the stacktrace
			 * for debuggging.
			 */
			if ((expectedFrames != ste.length) || !ste[0].getMethodName().equals(expectedMethodName)) {
				for (StackTraceElement st : ste) {
					System.out.println(st);
				}
			}

			AssertJUnit.assertTrue(
					"Expected " + expectedFrames + " frames, got " + ste.length,
					(expectedFrames == ste.length));

			AssertJUnit.assertTrue(
					"Expected top frame to be " + expectedMethodName + ", got " + ste[0].getMethodName(),
					ste[0].getMethodName().equals(expectedMethodName));

			LockSupport.unpark(t);
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testThread2_state = false;

	@Test
	public void test_RunningVirtualThreadGetStackTrace() {
		try {
			/* The expected frame count is based on test's callstack */
			int expectedFrames = 2;
			String expectedClassName = "org.openj9.test.jep425.VirtualThreadTests";

			Thread t = Thread.ofVirtual().name("running-stackwalker").start(() -> {
				testThread2_state = true;
				while (testThread2_state) {
					// busy wait
				}
			});

			while (!testThread2_state) {
				Thread.sleep(10);
			}

			StackTraceElement[] ste = t.getStackTrace();

			/* If the stacktrace doesn't match the expected result, then print out the stacktrace
			 * for debuggging.
			 */
			if ((expectedFrames != ste.length) || !ste[0].getClassName().equals(expectedClassName)) {
				for (StackTraceElement st : ste) {
					System.out.println(st);
				}
			}

			Assert.assertEquals(ste.length, expectedFrames);
			Assert.assertEquals(ste[0].getClassName(), expectedClassName);

			testThread2_state = false;
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static void checkState(Class<?> vthreadCls, String fieldName, int expectedValue) throws Exception {
		Field field = vthreadCls.getDeclaredField(fieldName);

		AssertJUnit.assertTrue(Modifier.isStatic(field.getModifiers()));
		Assert.assertEquals(field.getType(), int.class);

		field.setAccessible(true);

		int value = field.getInt(null);

		if (value != expectedValue) {
			Assert.fail(
					String.format(
							"VirtualThread.%s (%d) does not match JVMTI_VTHREAD_STATE_%s (%d)",
							fieldName, value, fieldName, expectedValue));
		}
	}

	@Test
	public void test_verifyJVMTIMacros() {
		final int JVMTI_VTHREAD_STATE_NEW = 0;
		final int JVMTI_VTHREAD_STATE_STARTED = 1;
		final int JVMTI_VTHREAD_STATE_RUNNING = 2;
		final int JVMTI_VTHREAD_STATE_PARKING = 3;
		final int JVMTI_VTHREAD_STATE_PARKED = 4;
		final int JVMTI_VTHREAD_STATE_PINNED = 5;
		final int JVMTI_VTHREAD_STATE_TIMED_PARKING = 6;
		final int JVMTI_VTHREAD_STATE_TIMED_PARKED = 7;
		final int JVMTI_VTHREAD_STATE_TIMED_PINNED = 8;
		final int JVMTI_VTHREAD_STATE_UNPARKED = 9;
		final int JVMTI_VTHREAD_STATE_YIELDING = 10;
		final int JVMTI_VTHREAD_STATE_YIELDED = 11;
		final int JVMTI_VTHREAD_STATE_TERMINATED = 99;
		final int JVMTI_VTHREAD_STATE_SUSPENDED = 1 << 8;

		try {
			Class<?> vthreadCls = Class.forName("java.lang.VirtualThread");

			checkState(vthreadCls, "NEW", JVMTI_VTHREAD_STATE_NEW);
			checkState(vthreadCls, "STARTED", JVMTI_VTHREAD_STATE_STARTED);
			checkState(vthreadCls, "RUNNING", JVMTI_VTHREAD_STATE_RUNNING);
			checkState(vthreadCls, "PARKING", JVMTI_VTHREAD_STATE_PARKING);
			checkState(vthreadCls, "PARKED", JVMTI_VTHREAD_STATE_PARKED);
			checkState(vthreadCls, "PINNED", JVMTI_VTHREAD_STATE_PINNED);
			checkState(vthreadCls, "TIMED_PARKING", JVMTI_VTHREAD_STATE_TIMED_PARKING);
			checkState(vthreadCls, "TIMED_PARKED", JVMTI_VTHREAD_STATE_TIMED_PARKED);
			checkState(vthreadCls, "TIMED_PINNED", JVMTI_VTHREAD_STATE_TIMED_PINNED);
			checkState(vthreadCls, "UNPARKED", JVMTI_VTHREAD_STATE_UNPARKED);
			checkState(vthreadCls, "YIELDING", JVMTI_VTHREAD_STATE_YIELDING);
			checkState(vthreadCls, "YIELDED", JVMTI_VTHREAD_STATE_YIELDED);
			checkState(vthreadCls, "TERMINATED", JVMTI_VTHREAD_STATE_TERMINATED);

			if (VersionCheck.major() <= 26) {
				checkState(vthreadCls, "SUSPENDED", JVMTI_VTHREAD_STATE_SUSPENDED);
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured: " + e.getMessage(), e);
		}
	}
}
