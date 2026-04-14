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

import org.testng.log4testng.Logger;

/**
 * This class is used to extract info from !j9module <address> DDR extension output.
 */
public class J9ModuleOutputParser {
	private static final Logger log = Logger.getLogger(J9ModuleOutputParser.class);

	/**
	 * This method extracts the module name from !j9module output.
	 *
	 * Sample output:
	 *   Module name: java.base
	 *
	 * @param j9ModuleOutput output of !j9module <address>
	 * @return module name (e.g. "java.base"), or null if not found
	 */
	public static String getModuleName(String j9ModuleOutput)
	{
		if (null == j9ModuleOutput) {
			log.error("!j9module output is null");
		} else {
			String[] outputLines = j9ModuleOutput.split(Constants.NL);
			for (String aLine : outputLines) {
				int index = aLine.indexOf("Module name:");
				if (index != -1) {
					return aLine.substring(index + "Module name:".length()).trim();
				}
			}
		}
		return null;
	}

}
