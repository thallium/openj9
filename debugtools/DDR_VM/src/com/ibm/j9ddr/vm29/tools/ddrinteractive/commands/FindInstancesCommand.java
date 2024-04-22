/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
 *******************************************************************************/
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectHeapIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class FindInstancesCommand extends Command {
	public FindInstancesCommand() {
		addCommand("findinstances", "[-s] <className> | -h | -help", "Find all instances of a given class");
	}

	final String helpMessage = "Usage:\n" +
								"\t!findinstances [-s] <className>\t\tFind all instances of a given class, use the -s flag to also include sub-classes\n";

	private static J9ClassPointer findClassByName(J9JavaVMPointer vm, String name) throws CorruptDataException {
		ClassSegmentIterator iterator = new ClassSegmentIterator(vm.classMemorySegments());
		while (iterator.hasNext()) {
			J9ClassPointer classRef = (J9ClassPointer)iterator.next();
			if (J9ClassHelper.getJavaName(classRef).equals(name)) {
				return classRef;
			}
		}
		return null;
	}

	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		String className = "";
		boolean includeSubclass = false;

		if (args.length == 1) {
			if (args[0].equals("-h") || args[0].equals("-help")) {
				out.print(helpMessage);
			} else {
				className = args[0];
			}
		} else if (args.length == 2) {
			if (!args[0].equals("-s")) {
				out.print(helpMessage);
				return;
			} else {
				className = args[1];
				includeSubclass = true;
			}
		} else {
			out.print(helpMessage);
			return;
		}

		try {
			final J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9ClassPointer targetClazz = findClassByName(vm, className);
			if (targetClazz == null) {
				out.println("Invalid class name or the class isn't loaded.");
				return;
			}
			GCHeapRegionIterator regions = GCHeapRegionIterator.from();
			while (regions.hasNext()) {
				GCHeapRegionDescriptor region = regions.next();
				GCObjectHeapIterator heapIterator = region.objectIterator(true, true);
				while (heapIterator.hasNext()) {
					J9ObjectPointer object = heapIterator.next();
					J9ClassPointer clazz = J9ObjectHelper.clazz(object);
					if (includeSubclass) {
						if (clazz.notNull() && J9ClassHelper.isSameOrSuperClassOf(targetClazz, clazz)) {
							out.printf("!j9object %s - %s", object.getHexAddress(), J9ClassHelper.getName(clazz));
						}
					} else {
						if (targetClazz.eq(clazz)) {
							out.println("!j9object " + object.getHexAddress());
						}
					}
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
