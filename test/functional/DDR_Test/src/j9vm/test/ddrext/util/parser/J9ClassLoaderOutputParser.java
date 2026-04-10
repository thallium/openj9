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
package j9vm.test.ddrext.util.parser;

import j9vm.test.ddrext.Constants;

/**
 * This class is used to extract info from !j9classloader <address> DDR extension output.
 */
public class J9ClassLoaderOutputParser {
	private static final String FIELDSIGNATURE_CLASSHASHTABLE = "* classHashTable";
	private static final String FIELDSIGNATURE_MODULEHASHTABLE = "* moduleHashTable";

	/**
	 * This method finds the address of the classHashTable field from !j9classloader output.
	 *
	 * Sample output:
	 *   0x28: struct J9HashTable * classHashTable = !j9hashtable 0x00007F9DE00F6EC0
	 *
	 * @param j9ClassLoaderOutput output of !j9classloader <address>
	 * @return hex address of the J9HashTable, or null if not found
	 */
	public static String getClassHashTableAddress(String j9ClassLoaderOutput)
	{
		return ParserUtil.getFieldAddressOrValue(FIELDSIGNATURE_CLASSHASHTABLE, Constants.J9HASHTABLE_CMD, j9ClassLoaderOutput);
	}

	/**
	 * This method finds the address of the moduleHashTable field from !j9classloader output.
	 *
	 * Sample output:
	 *   0x38: struct J9HashTable * moduleHashTable = !j9hashtable 0x00007F4E880F84E0
	 *
	 * @param j9ClassLoaderOutput output of !j9classloader <address>
	 * @return hex address of the J9HashTable, or null if not found
	 */
	public static String getModuleHashTableAddress(String j9ClassLoaderOutput)
	{
		return ParserUtil.getFieldAddressOrValue(FIELDSIGNATURE_MODULEHASHTABLE, Constants.J9HASHTABLE_CMD, j9ClassLoaderOutput);
	}

}
