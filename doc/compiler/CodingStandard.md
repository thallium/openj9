<!--
Copyright IBM Corp. and others 2026

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution
and is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following Secondary
Licenses when the conditions for such availability set forth in the
Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
version 2 with the GNU Classpath Exception [1] and GNU General Public
License, version 2 with the OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

# Coding Format

The C/C++ code under the `runtime/compiler` directory is formatted using
`clang-format`. The format is specified by the
[.clang-format](https://github.com/eclipse-openj9/openj9-omr/blob/openj9/compiler/.clang-format)
file from the Eclipse OMR project, specifically Eclipse OpenJ9's fork. Every
commit must be formatted using this tool. The OpenJ9 project runs a linter to
check the coding format of the compiler component.

Take a look at the
[OMR documentation](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/CodingStandard.md)
for how to use `clang-format`; the steps will need to be modified to account
for the fact that in OpenJ9, the compiler sits in the `runtime/compiler`
directory.
