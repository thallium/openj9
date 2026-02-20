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
package org.openj9.test;

import java.io.FileWriter;
import java.io.PrintWriter;

/**
 * Utility to write JVM options to Liberty's jvm.options file.
 * Usage: java LibertyJvmOptions <path-to-jvm.options> <option1> [option2] ...
 */
public class LibertyJvmOptions {
    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("Usage: LibertyJvmOptions <jvm.options-path> <option1> [option2] ...");
            System.exit(1);
        }

        String jvmOptionsPath = args[0];

        try (PrintWriter writer = new PrintWriter(new FileWriter(jvmOptionsPath))) {
            // Write each JVM option on a separate line
            for (int i = 1; i < args.length; i++) {
                writer.println(args[i]);
            }
            System.out.println("JVM options configured successfully at: " + jvmOptionsPath);
            System.out.println("JVM options configured");
        } catch (Exception e) {
            System.err.println("Failed to write JVM options: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}

// Made with Bob
