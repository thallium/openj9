/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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

#include "jni.h"
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"
#include "rommeth.h"
#include "omrlinkedlist.h"
#include "VMHelpers.hpp"

extern "C" {

#if JAVA_SPEC_VERSION >= 16
void JNICALL
Java_jdk_internal_misc_ScopedMemoryAccess_registerNatives(JNIEnv *env, jclass clazz)
{
}

/**
 * For each thread, walk Java stack and look for the scope instance. Methods that can take and access a scope
 * instance are marked with the "@Scoped" extended modifier. If the scope instance is found in a method, that
 * method is accessing the memory segment associated with the scope and thus closing the scope will fail.
 */
#if JAVA_SPEC_VERSION >= 22
void JNICALL
Java_jdk_internal_misc_ScopedMemoryAccess_closeScope0(JNIEnv *env, jobject instance, jobject scope, jobject error)
#elif (JAVA_SPEC_VERSION >= 19) && (JAVA_SPEC_VERSION <= 21) /* JAVA_SPEC_VERSION >= 22 */
jboolean JNICALL
Java_jdk_internal_misc_ScopedMemoryAccess_closeScope0(JNIEnv *env, jobject instance, jobject scope)
#else /* JAVA_SPEC_VERSION >= 22 */
jboolean JNICALL
Java_jdk_internal_misc_ScopedMemoryAccess_closeScope0(JNIEnv *env, jobject instance, jobject scope, jobject exception)
#endif /* JAVA_SPEC_VERSION >= 22 */
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
#if JAVA_SPEC_VERSION <= 21
	jboolean scopeFound = JNI_FALSE;
#endif /* JAVA_SPEC_VERSION <= 21 */

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == scope) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		vmFuncs->acquireExclusiveVMAccess(currentThread);
		J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
		j9object_t closeScopeObj = J9_JNI_UNWRAP_REFERENCE(scope);
#if JAVA_SPEC_VERSION >= 22
		j9object_t errorObj = J9_JNI_UNWRAP_REFERENCE(error);
		jobject closeScopeGlobalRef = NULL;
		jobject errorGlobalRef = NULL;
		I_64 closeScopeCount = 0;
		bool setNativeOOM = false;

		PORT_ACCESS_FROM_JAVAVM(vm);
#endif /* JAVA_SPEC_VERSION >= 22 */

		while (NULL != walkThread) {
			if (VM_VMHelpers::threadCanRunJavaCode(walkThread)) {
				if (vmFuncs->hasMemoryScope(walkThread, closeScopeObj)) {
					/* Scope found. */
#if JAVA_SPEC_VERSION >= 22
					/* Create global refs once so it can be safely shared across threads. */
					if (NULL == closeScopeGlobalRef) {
						closeScopeGlobalRef = vmFuncs->j9jni_createGlobalRef(env, closeScopeObj, JNI_FALSE);
						if (NULL == closeScopeGlobalRef) {
							setNativeOOM = true;
							break;
						}
					}

					if (NULL == errorGlobalRef) {
						errorGlobalRef = vmFuncs->j9jni_createGlobalRef(env, errorObj, JNI_FALSE);
						if (NULL == errorGlobalRef) {
							setNativeOOM = true;
							break;
						}
					}

					/* Allocate a per-thread node to queue this close request. */
					J9CloseScopeListNode *parentNode = (J9CloseScopeListNode *)j9mem_allocate_memory(sizeof(J9CloseScopeListNode), J9MEM_CATEGORY_VM);
					if (NULL == parentNode) {
						setNativeOOM = true;
						break;
					}

					/* Push close request onto the thread’s pending closeScope list. */
					parentNode->closeScope = closeScopeGlobalRef;
					parentNode->scopeError = errorGlobalRef;
					parentNode->next = walkThread->closeScopeList;
					walkThread->closeScopeList = parentNode;

					VM_VMHelpers::indicateAsyncMessagePending(walkThread);

					closeScopeCount += 1;
#else /* JAVA_SPEC_VERSION >= 22 */
					scopeFound = JNI_TRUE;
					break;
#endif /* JAVA_SPEC_VERSION >= 22 */
				}
			}

			walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
		}
#if JAVA_SPEC_VERSION >= 22
		if (setNativeOOM) {
			/* Error: rollback and cleanup created nodes. */
			if (closeScopeCount > 0) {
				walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
				while (NULL != walkThread) {
					J9CloseScopeListNode *head = walkThread->closeScopeList;
					if ((NULL != head)
					&& (closeScopeGlobalRef == head->closeScope)
					&& (errorGlobalRef == head->scopeError)
					) {
						walkThread->closeScopeList = head->next;
						j9mem_free_memory(head);

						closeScopeCount -= 1;
						if (0 == closeScopeCount) {
							break;
						}
					}

					walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
				}
			}

			if (NULL != closeScopeGlobalRef) {
				vmFuncs->j9jni_deleteGlobalRef(env, closeScopeGlobalRef, JNI_FALSE);
			}

			if (NULL != errorGlobalRef) {
				vmFuncs->j9jni_deleteGlobalRef(env, errorGlobalRef, JNI_FALSE);
			}
		} else {
			/* Success: store number of pending close notifications on the MemorySessionImpl object. */
			if (closeScopeCount > 0) {
				J9OBJECT_I64_STORE(currentThread, closeScopeObj, vm->closeScopeCountOffset, closeScopeCount);
			}
		}

#endif /* JAVA_SPEC_VERSION >= 22 */
		vmFuncs->releaseExclusiveVMAccess(currentThread);

#if JAVA_SPEC_VERSION >= 22
		if (setNativeOOM) {
			/*  Create exception after releasing exclusive VM access. */
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		}
#endif /* JAVA_SPEC_VERSION >= 22 */
	}

	vmFuncs->internalExitVMToJNI(currentThread);

#if JAVA_SPEC_VERSION <= 21
	return !scopeFound;
#endif /* JAVA_SPEC_VERSION <= 21 */
}
#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
