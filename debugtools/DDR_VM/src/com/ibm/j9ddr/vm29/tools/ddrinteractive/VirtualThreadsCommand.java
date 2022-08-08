/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * vthreads command lists all the virtual threads
 *
 * Example:
 * 			!vthread
 * Example output:
 * !continuationstack 0x00007fe78c0f9600 !j9vmcontinuation 0x00007fe78c0f9600 !j9object 0x0000000706401588 (Continuation) !j9object 0x0000000706400FB0 (VThread) - name1
 * !continuationstack 0x00007fe78c23aa80 !j9vmcontinuation 0x00007fe78c23aa80 !j9object 0x0000000706424F90 (Continuation) !j9object 0x0000000706424EF0 (VThread) - name2
 * !continuationstack 0x00007fe78c244ac0 !j9vmcontinuation 0x00007fe78c244ac0 !j9object 0x00000007064250D8 (Continuation) !j9object 0x0000000706425038 (VThread) - name3
 * .....
 */
public class VirtualThreadsCommand extends Command {
	public VirtualThreadsCommand() {
		addCommand("vthreads", "", "Lists virtual threads");
	}

	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (!JavaVersionHelper.ensureJava19AndUp(vm, out)) {
				return;
			}
			displayVirtualThreads(out);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	/**
	 * Prints all the live virtual threads
	 *
	 * @param out the PrintStream to write output to
	 */
	private void displayVirtualThreads(PrintStream out) throws  CorruptDataException {
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		PointerPointer mainVirtualThread = vm.liveVirtualThreadList();

		UDATA linkNextOffset = vm.virtualThreadLinkNextOffset();

		if (mainVirtualThread.notNull() && mainVirtualThread.at(0).notNull()) {
			/*
			 * liveVirtualThreadList is a circular doubly-linked list storing all the live virtual threads.
			 * The root node is a dummy virtual thread marking the start and the end of the list.
			 */
			J9ObjectPointer root = J9ObjectPointer.cast(mainVirtualThread.at(0));
			J9ObjectPointer node = ObjectReferencePointer.cast(root.addOffset(linkNextOffset)).at(0);
			J9ObjectFieldOffset nameOffset = J9ObjectHelper.getFieldOffset(node, "name", "Ljava/lang/String;");
			J9ObjectFieldOffset contOffset = J9ObjectHelper.getFieldOffset(node, "cont", "Ljdk/internal/vm/Continuation;");
			J9ObjectPointer cont = J9ObjectHelper.getObjectField(node, contOffset);
			J9ObjectFieldOffset vmRefOffset = J9ObjectHelper.getFieldOffset(cont, "vmRef", "J");

			while (!node.eq(root)) {
				J9ObjectPointer name = J9ObjectHelper.getObjectField(node, nameOffset);
				cont = J9ObjectHelper.getObjectField(node, contOffset);
				long vmRef = J9ObjectHelper.getLongField(cont, vmRefOffset);
				String formattedOutput = String.format(
								"!continuationstack 0x%016x !j9vmcontinuation 0x%016x !j9object %s (Continuation) !j9object %s (VThread) - %s",
								vmRef,
								vmRef,
								cont.getHexAddress(),
								node.getHexAddress(),
								J9ObjectHelper.stringValue(name));
				out.println(formattedOutput);
				node = ObjectReferencePointer.cast(node.addOffset(linkNextOffset)).at(0);
			}
		}
	}
}
