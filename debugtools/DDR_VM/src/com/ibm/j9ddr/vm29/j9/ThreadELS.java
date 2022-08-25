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
package com.ibm.j9ddr.vm29.j9;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9I2JStatePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMEntryLocalStoragePointer;

public class ThreadELS implements EntryLocalStorage {
    private J9VMEntryLocalStoragePointer target;

    public ThreadELS(J9VMEntryLocalStoragePointer target) {
        this.target = target;
    }

    @Override
    public EntryLocalStorage oldEntryLocalStorage() throws CorruptDataException {
        return new ThreadELS(target.oldEntryLocalStorage());
    }

    @Override
    public UDATAPointer jitGlobalStorageBase() throws CorruptDataException {
        return target.jitGlobalStorageBase();
    }

    @Override
    public UDATAPointer jitFPRegisterStorageBase() throws CorruptDataException {
        return target.jitFPRegisterStorageBase();
    }

    @Override
    public J9I2JStatePointer i2jState() throws CorruptDataException {
        return target.i2jState();
    }

    @Override
    public PointerPointer i2jStateEA() throws CorruptDataException {
        return target.i2jStateEA();
    }

    @Override
    public String getHexAddress() {
        return target.getHexAddress();
    }

    @Override
    public long getAddress() {
        return target.getAddress();
    }

    @Override
    public boolean notNull() {
        return target.notNull();
    }
}
