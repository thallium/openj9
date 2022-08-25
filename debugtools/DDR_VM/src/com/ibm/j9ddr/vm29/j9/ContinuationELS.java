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
import com.ibm.j9ddr.vm29.pointer.generated.J9VMContinuationPointer;

public class ContinuationELS implements EntryLocalStorage {
    private J9VMContinuationPointer target;

	public ContinuationELS(J9VMContinuationPointer target) {
		this.target = target;
	}

    @Override
    public EntryLocalStorage oldEntryLocalStorage() throws CorruptDataException {
        try {
            return new ThreadELS(target.oldEntryLocalStorage());
        } catch (NoSuchFieldException e) {
            throw new CorruptDataException(e);
        }
    }

    @Override
    public UDATAPointer jitGlobalStorageBase() throws CorruptDataException {
        try {
            return UDATAPointer.cast((long)target.jitGPRs().getAddress());
        } catch (NoSuchFieldException e) {
            throw new CorruptDataException(e);
        }
    }

    @Override
    public UDATAPointer jitFPRegisterStorageBase() throws CorruptDataException {
        throw new UnsupportedOperationException();
    }

    @Override
    public J9I2JStatePointer i2jState() throws CorruptDataException {
        try {
            return target.i2jState();
        } catch (NoSuchFieldException e) {
            throw new CorruptDataException(e);
        }
    }

    @Override
    public PointerPointer i2jStateEA() throws CorruptDataException {
        try {
            return target.i2jStateEA();
        } catch (NoSuchFieldException e) {
            throw new CorruptDataException(e);
        }
    }

    @Override
    public String getHexAddress() {
		return J9VMContinuationPointer.NULL.getHexAddress();
    }

    @Override
    public long getAddress() {
        return 0;
    }

    @Override
    public boolean notNull() {
        return target.notNull();
    }
}
