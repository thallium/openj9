/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.jvmti.tests.eventMethodEntryGrow;

import java.util.concurrent.locks.LockSupport;

public class emeng001 {
	public native void emeng001NativeMethod(String o, int i, long l, String o2);

	public static boolean pass = false;
	public static boolean stackOverFlow = false;
	public static boolean called = false;
	public static emeng001 receiverStatic;
	public static String oStatic = "hey";
	public static int iStatic = 42;
	public static long lStatic = -1L;
	public static String o2Static = "there";


	public static int fibonacci(int x) {
		if (x < 3) {
			return 1;
		}
		return (fibonacci(x - 1) + fibonacci(x - 2));
	}
	
	public void foo() {
		try {
			throw new RuntimeException();
		} catch (RuntimeException e) {

		}
	}
	
	public boolean testMethodEntryWithStackGrow()
	{
		try {
			throw new RuntimeException();
		} catch (RuntimeException e) {

		}
		// Thread t2 = new Thread(() -> {
		// 	while (Thread.temp == null) {
		// 		long x = 0;
		// 		for (int i = 0; i < 10000; i++) { x += 1; }
		// 	}
		// 	getStackTrace();
		// 	// do something to trigger an event with VirtualThread.tempField
		// 	// in jvmti event callback, gets stacktrace using the vthread object from tempField
		// 	Thread.temp= null;
		// });
		// t2.start();
		Thread thread = Thread.ofVirtual().name("test-vthread").start(() -> {
			fibonacci(1);
			LockSupport.parkNanos(1000000);
		});
		try {
			thread.join();
			// t2.join();
		} catch (InterruptedException e) {

		}
		return true;
	}
	
	public String helpMethodEntryWithStackGrow()
	{
		return "Test growing the stack during a MethodEntry event on a native method";
	}
}
