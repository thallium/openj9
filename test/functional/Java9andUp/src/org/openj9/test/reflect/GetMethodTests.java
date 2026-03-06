/*
 * Copyright IBM Corp. and others 2026
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
package org.openj9.test.reflect;

import org.testng.annotations.Test;
import static org.testng.Assert.*;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/*
 * Tests for Class.getMethod() with interface methods (Java 9+ feature).
 */
@Test(groups = { "level.sanity" })
public class GetMethodTests {

	/*
	 * Test that Class.getMethod() throws NoSuchMethodException for
	 * private static methods in interfaces.
	 */
	@Test
	public void testGetMethod_InterfacePrivateStatic() {
		assertThrows(NoSuchMethodException.class,
			() -> TestInterfaceForGetMethod.class.getMethod("privateStaticMethod"));
	}

	/*
	 * Test that Class.getMethod() throws NoSuchMethodException for
	 * private non-static methods in interfaces.
	 */
	@Test
	public void testGetMethod_InterfacePrivateNonStatic() {
		assertThrows(NoSuchMethodException.class,
			() -> TestInterfaceForGetMethod.class.getMethod("privateNonStaticMethod"));
	}

	/*
	 * Test that Class.getMethod() successfully returns a Method for
	 * public static methods in interfaces.
	 */
	@Test
	public void testGetMethod_InterfacePublicStatic() throws NoSuchMethodException {
		Method method = TestInterfaceForGetMethod.class.getMethod("publicStaticMethod");
		assertEquals("publicStaticMethod", method.getName());
		assertEquals(TestInterfaceForGetMethod.class, method.getDeclaringClass());
		assertEquals(void.class, method.getReturnType());
		assertEquals(0, method.getParameterCount());
		assertTrue(Modifier.isPublic(method.getModifiers()));
		assertTrue(Modifier.isStatic(method.getModifiers()));
	}

	/*
	 * Test that Class.getMethod() successfully returns a Method for
	 * public non-static methods in interfaces.
	 */
	@Test
	public void testGetMethod_InterfacePublicNonStatic() throws NoSuchMethodException {
		Method method = TestInterfaceForGetMethod.class.getMethod("publicNonStaticMethod");
		assertEquals("publicNonStaticMethod", method.getName());
		assertEquals(TestInterfaceForGetMethod.class, method.getDeclaringClass());
		assertEquals(void.class, method.getReturnType());
		assertEquals(0, method.getParameterCount());
		assertTrue(Modifier.isPublic(method.getModifiers()));
		assertTrue(Modifier.isAbstract(method.getModifiers()));
	}

	/*
	 * Test that Class.getMethod() successfully returns a Method for
	 * default methods in interfaces.
	 */
	@Test
	public void testGetMethod_InterfaceDefault() throws NoSuchMethodException {
		Method method = TestInterfaceForGetMethod.class.getMethod("defaultMethod");
		assertEquals("defaultMethod", method.getName());
		assertEquals(TestInterfaceForGetMethod.class, method.getDeclaringClass());
		assertEquals(void.class, method.getReturnType());
		assertEquals(0, method.getParameterCount());
		assertTrue(Modifier.isPublic(method.getModifiers()));
	}

}
