// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project
#include <jni.h>
#include <media/AudioSystem.h>
#include <nativehelper/scoped_utf_chars.h>
#include <utils/Errors.h>

extern "C" JNIEXPORT jint JNICALL
Java_de_diamaneos_callaudio_AuthorizationTest_setParametersNative(
        JNIEnv* env, jclass, jstring value) {
    const ScopedUtfChars parameters(env, value);
    if (parameters.c_str() == nullptr) return android::BAD_VALUE;
    // Execute as this test application's UID; no shell identity or permission adoption.
    return android::AudioSystem::setParameters(android::String8(parameters.c_str()));
}
