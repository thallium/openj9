################################################################################
# Copyright IBM Corp. and others 2017
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
################################################################################

set(OMR_ENHANCED_WARNINGS OFF)
add_tracegen(j9vrb.tdf)

set(verbose_sources
	errormessage_internal.c
	errormessagebuffer.c
	errormessageframeworkcfr.c
	errormessageframeworkrtv.c
	errormessagehelper.c
	verbose.c

	${j9vm_SOURCE_DIR}/vm/swalk.c
	${j9vm_SOURCE_DIR}/vm/linearswalk.c

	${j9vm_SOURCE_DIR}/codert_vm/jswalk.c #IF J9VM_INTERP_NATIVE_SUPPORT
	${j9vm_SOURCE_DIR}/jit_vm/ctsupport.cpp #IF J9VM_INTERP_USE_SPLIT_SIDE_TABLES
	${j9vm_SOURCE_DIR}/compiler/runtime/MethodMetaData.c

	${CMAKE_CURRENT_BINARY_DIR}/ut_j9vrb.c
)

j9vm_add_library(j9vrb SHARED
	OUTPUT_NAME j9vrb${J9VM_VERSION_SUFFIX}
	${verbose_sources}
)

# Note: OMR_TOOLCONFIG=gnu also captures gnu-like compilers, such as clang
if(OMR_TOOLCONFIG STREQUAL "gnu")
	set_property(SOURCE ../compiler/runtime/MethodMetaData.c APPEND_STRING PROPERTY COMPILE_FLAGS " -std=gnu89")
endif()

target_include_directories(j9vrb
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}
		${j9vm_SOURCE_DIR}/compiler/runtime
		${j9vm_SOURCE_DIR}/compiler/codegen
		${omr_SOURCE_DIR}/compiler/codegen
		${omr_SOURCE_DIR}/compiler
		${j9vm_SOURCE_DIR}/compiler
		${j9vm_SOURCE_DIR}/shared_common/include
)

target_link_libraries(j9vrb
	PRIVATE
		j9vm_interface
		j9vm_gc_includes
		j9vm_compiler_defines

		j9stackmap
		j9verutil
		j9pool
		j9modronstartup
		j9gcvrbjava
		j9gcvrbold
		j9gcvrbevents
		j9gcvrbhdlrstandardjava
		j9gcvrbhdlrrealtime
		j9gcvrbhdlrvlhgc
		j9zip
		j9utilcore
		j9gc
		j9gctrc
		j9gcapi
		j9gctrcstandard
		j9gctrcvlhgc
)

target_compile_definitions(j9vrb
	PRIVATE
		-DJ9VM_INTERP_STACKWALK_TRACING
		-DJ9VM_INTERP_LINEAR_STACKWALK_TRACING
)

omr_add_exports(j9vrb
	JVM_OnLoad
	JVM_OnUnload
	J9VMDllMain
)

target_enable_ddr(j9vrb GLOB_HEADERS)
ddr_set_add_targets(j9ddr j9vrb)

install(
	TARGETS j9vrb
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)

if(OMR_MIXED_REFERENCES_MODE_STATIC)
	j9vm_add_library(j9vrb_full SHARED
		OUTPUT_NAME j9vrb_full${J9VM_VERSION_SUFFIX}
		${verbose_sources}
	)

	target_include_directories(j9vrb_full
		PRIVATE
			${CMAKE_CURRENT_BINARY_DIR}
			${j9vm_SOURCE_DIR}/compiler/runtime
			${j9vm_SOURCE_DIR}/compiler/codegen
			${omr_SOURCE_DIR}/compiler/codegen
			${omr_SOURCE_DIR}/compiler
			${j9vm_SOURCE_DIR}/compiler
			${j9vm_SOURCE_DIR}/shared_common/include
	)

	target_link_libraries(j9vrb_full
		PRIVATE
			j9vm_interface
			j9vm_gc_includes
			j9vm_compiler_defines

			j9stackmap
			j9verutil
			j9pool
			j9modronstartup_full
			j9gcvrbjava_full
			j9gcvrbold_full
			j9gcvrbevents_full
			j9gcvrbhdlrstandardjava_full
			j9gcvrbhdlrrealtime_full
			j9gcvrbhdlrvlhgc_full
			j9zip
			j9utilcore
			j9gc
			j9gctrc
			j9gcapi
			j9gctrcstandard
			j9gctrcvlhgc
	)

	target_compile_definitions(j9vrb_full
		PRIVATE
			-DJ9VM_INTERP_STACKWALK_TRACING
			-DJ9VM_INTERP_LINEAR_STACKWALK_TRACING
	)

	omr_add_exports(j9vrb_full
		JVM_OnLoad
		JVM_OnUnload
		J9VMDllMain
	)

	install(
		TARGETS j9vrb_full
		LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
		RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
	)
endif()
