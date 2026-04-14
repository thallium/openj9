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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.HashTable;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Implementation of DDR extension !walkj9hashtable.
 *
 * This extension takes a J9HashTable address and an optional type argument,
 * and prints the address of each element in the hashtable as a runnable DDR command.
 * The type argument is used to derive the DDR command prefix to prepend to each element's address.
 * If not provided, the default DDR command prefix is "!j9x".
 */
public class WalkJ9HashTableCommand extends Command
{
	private static final String POINTER_MARKER = "*";

	/**
	 * Constructor
	 */
	public WalkJ9HashTableCommand()
	{
		addCommand("walkj9hashtable", "<address> [<type>]", "Walks the elements of a J9HashTable");
	}

	/**
	 * Prints the usage for the walkj9hashtable command.
	 *
	 * @param out  the PrintStream the usage statement prints to
	 */
	private void printUsage(PrintStream out)
	{
		out.println("walkj9hashtable <address> [<type>] - Walks the elements of a J9HashTable");
	}

	/**
	 * Run method for !walkj9hashtable extension.
	 *
	 * @param command  !walkj9hashtable
	 * @param args     args passed by !walkj9hashtable extension
	 * @param context  Context
	 * @param out      PrintStream
	 * @throws DDRInteractiveCommandException
	 */
	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		if ((0 == args.length) || (2 < args.length)) {
			printUsage(out);
			return;
		}

		long address = CommandUtils.parsePointer(args[0], J9BuildFlags.J9VM_ENV_DATA64);

		/* Parse the optional type argument. */
		String typeName = (2 == args.length) ? args[1].trim() : null;

		/* Set the default values. */
		boolean isInline = true;
		String ddrCommand = "!j9x";

		if (null != typeName) {
			String baseTypeName;
			if (typeName.endsWith(POINTER_MARKER)) {
				isInline = false;
				baseTypeName = typeName.substring(0, typeName.length() - POINTER_MARKER.length());
			} else {
				baseTypeName = typeName;
			}

			baseTypeName = baseTypeName.toLowerCase();

			/*
			 * Validate: check that the derived DDR command is actually registered in
			 * this context. This catches typos (e.g. "j9claas*") early with a clear
			 * error message.
			 */
			if (!context.getCommandNames().contains(baseTypeName)) {
				throw new DDRInteractiveCommandException(
						"Unrecognized type '" + typeName + "'");
			}

			ddrCommand = "!" + baseTypeName;
		}

		out.format("J9HashTable at 0x%s%n{%n",
				CommandUtils.longToBigInteger(address).toString(CommandUtils.RADIX_HEXADECIMAL));

		walkJ9HashTable(address, isInline, ddrCommand, out);
		out.println("}");
	}

	/**
	 * Iterates all entries in the hashtable and prints each entry's address
	 * as a runnable DDR command.
	 *
	 * @param address     address of the J9HashTable
	 * @param isInline    true if entries are stored inline in the slot;
	 *                    false if the slot holds a pointer to the entry
	 * @param ddrCommand  DDR command prefix to prepend
	 * @param out         print stream
	 * @throws DDRInteractiveCommandException
	 */
	private void walkJ9HashTable(long address, boolean isInline, String ddrCommand, PrintStream out)
			throws DDRInteractiveCommandException
	{
		try {
			J9HashTablePointer j9hashtable = J9HashTablePointer.cast(address);

			HashTable.HashEqualFunction<VoidPointer> equalFn = (e1, e2) -> e1.eq(e2);
			HashTable.HashFunction<VoidPointer> hashFn = entry -> UDATA.cast(entry);

			HashTable<VoidPointer> hashTable = HashTable.fromJ9HashTable(
					j9hashtable, isInline, VoidPointer.class, equalFn, hashFn);

			for (SlotIterator<VoidPointer> iterator = hashTable.iterator(); iterator.hasNext();) {
				VoidPointer currentElement = iterator.next();
				out.format("  %s %s%n", ddrCommand, currentElement.getHexAddress());
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException("Either address is not a valid J9HashTable address or J9HashTable is corrupted.");
		}
	}
}
