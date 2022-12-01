/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include <string.h>
#include "jvmti_test.h"
#include "ibmjvmti.h"

static agentEnv * env;

static void JNICALL cbMethodEntry(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method);
static void JNICALL cbException(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jobject exception, jmethodID catch_method, jlocation catch_location);
static void JNICALL cbMethodExit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value);
static void JNICALL cbFramePop(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception);
static void JNICALL cbExceptionCatch(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jobject exception);
static void JNICALL cbFieldModification(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jclass field_klass, jobject object, jfieldID field, char signature_type, jvalue new_value);

jint JNICALL
emeng001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err;

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_generate_method_entry_events = 1;
	capabilities.can_generate_exception_events = 1;
	capabilities.can_generate_method_exit_events = 1;
	capabilities.can_generate_frame_pop_events = 1;
	capabilities.can_generate_field_modification_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}

	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.MethodEntry = cbMethodEntry;
	callbacks.Exception = cbException;
	callbacks.ExceptionCatch = cbExceptionCatch;
	callbacks.MethodExit = cbMethodExit;
	callbacks.FramePop = cbFramePop;
	callbacks.FieldModification = cbFieldModification;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callback for MethodEntry callback");
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, NULL);
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION, NULL);
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION_CATCH, NULL);
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_FRAME_POP, NULL);
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_FIELD_MODIFICATION, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable MethodEntry event");
		return JNI_ERR;
	}

	return JNI_OK;
}

int parked = 0;
int cnt = 0;

static void JNICALL
cbMethodEntry(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method)
{
	char *name_ptr = NULL;

	jvmtiError err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, NULL, NULL);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetMethodName");
		return;
    }
	// if (0 == strcmp(name_ptr, "getStackTrace")) {
	// }

	jboolean isVirtual = (*jni_env)->IsVirtualThread(jni_env, thread);

	if (JNI_TRUE == isVirtual) {
		if (0 == strcmp(name_ptr, "scheduleUnpark") && parked == 0) {
			jclass vthreadClass = (*jni_env)->FindClass(jni_env, "java/lang/Thread");
			if (vthreadClass != NULL) {
				jfieldID temp = (*jni_env)->GetStaticFieldID(jni_env, vthreadClass, "temp", "Ljava/lang/VirtualThread;");
				if (temp != NULL) {
					err = (*jvmti_env)->SetFieldModificationWatch(jvmti_env, vthreadClass, temp);
				} else {
					printf("temp field cannot be found!\n");
				}
			} else {
				printf("VirtualThread class cannot be found!\n");
			}
			parked = 1;
		}
		// if (0 == strcmp(name_ptr, "yieldContinuation")) {
		// 	parked = 0;
		// }
		if (parked) {
			printf("Entering: %s\n", name_ptr);
		}
		// jvmtiThreadInfo info;
		// jvmtiError err;
		//
		// err = (*jvmti_env)->GetThreadInfo(jvmti_env, thread, &info);
		// printf("%s %s\n", info.name, name_ptr);
		// (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)info.name);
	}

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)name_ptr);

	return;
}

static void JNICALL
cbMethodExit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value) {
	char *name_ptr = NULL;

	jvmtiError err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, NULL, NULL);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetMethodName");
		return;
    }
	jboolean isVirtual = (*jni_env)->IsVirtualThread(jni_env, thread);

	if (JNI_TRUE == isVirtual) {
		if (parked) {
			printf("Exiting: %s\n", name_ptr);
		}
		if (0 == strcmp(name_ptr, "yieldContinuation")) {
			parked = 0;
		}
	}
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)name_ptr);
}

static void JNICALL
cbException(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jobject exception, jmethodID catch_method, jlocation catch_location) {
	printf("Exception event!\n");
}
static void JNICALL cbExceptionCatch(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jobject exception) {
	printf("Exception catched!\n");
}

static void JNICALL cbFramePop(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception) {
	char *name_ptr = NULL;

	jvmtiError err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, NULL, NULL);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetMethodName");
		return;
    }
	jboolean isVirtual = (*jni_env)->IsVirtualThread(jni_env, thread);

	if (JNI_TRUE == isVirtual) {
		if (parked) {
			printf("Poping frame: %s\n", name_ptr);
		}
		if (0 == strcmp(name_ptr, "yieldContinuation")) {
			parked = 0;
		}
	}
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)name_ptr);
}
static void JNICALL cbFieldModification(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location, jclass field_klass, jobject object, jfieldID field, char signature_type, jvalue new_value) {
	printf("Field mod\n");
	// jvmtiFrameInfo frames[100];
	// jint count;
	// jvmtiError err = (*jvmti_env)->GetStackTrace(jvmti_env, thread, 0, 100, frames, &count);
	// printf("[Stacktrace]\n");
	// for (int i = 0; i < count; i++) {
	// 	char *methodName;
	// 	err = (*jvmti_env)->GetMethodName(jvmti_env, frames[i].method, &methodName, NULL, NULL);
	// 	printf("%s\n", methodName);
	// 	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)methodName);
	// }
	// printf("\n");

	return;
}

void JNICALL
Java_com_ibm_jvmti_tests_eventMethodEntryGrow_emeng001_emeng001NativeMethod(JNIEnv *env, jobject rcv, jobject o, jint i, jlong l, jobject o2)
{
	jclass c = (*env)->GetObjectClass(env, rcv);
	if (NULL != c) {
		jmethodID mid = (*env)->GetMethodID(env, c, "verifyValues", "(Ljava/lang/String;IJLjava/lang/String;)V");
		if (NULL != mid) {
			(*env)->CallVoidMethod(env, rcv, mid, o, i, l, o2);
		}
	}
}
