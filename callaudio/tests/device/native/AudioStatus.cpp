// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project
#include <android/binder_auto_utils.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_parcel.h>
#include <jni.h>
#include <nativehelper/scoped_utf_chars.h>

namespace {
// IAudioFlingerService.aidl: setParameters is the eighteenth method at the
// pinned Android17 interface. This is a wire ABI index, not a scheduling value.
constexpr transaction_code_t kSetParameters = FIRST_CALL_TRANSACTION + 17;
constexpr int32_t kGlobalAudioHandle = 0; // AUDIO_IO_HANDLE_NONE
void* create(void*) { return nullptr; }
void destroy(void*) {}
binder_status_t unsupported(AIBinder*, transaction_code_t, const AParcel*, AParcel*) {
    return STATUS_UNKNOWN_TRANSACTION;
}
}
extern "C" JNIEXPORT jint JNICALL
Java_de_diamaneos_callaudio_AuthorizationTest_setParametersNative(
        JNIEnv* env, jclass, jobject binder, jstring value) {
    const ScopedUtfChars parameters(env, value);
    if (parameters.c_str() == nullptr || binder == nullptr) return STATUS_BAD_VALUE;
    ndk::SpAIBinder remote(AIBinder_fromJavaBinder(env, binder));
    static AIBinder_Class* client = AIBinder_Class_define(
            "android.media.IAudioFlingerService", create, destroy, unsupported);
    if (!remote.get() || !AIBinder_associateClass(remote.get(), client)) return STATUS_BAD_TYPE;
    ndk::ScopedAParcel input;
    binder_status_t status = AIBinder_prepareTransaction(remote.get(), input.getR());
    if (status != STATUS_OK) return status;
    status = AParcel_writeInt32(input.get(), kGlobalAudioHandle);
    if (status != STATUS_OK) return status;
    status = AParcel_writeString(input.get(), parameters.c_str(), parameters.size());
    if (status != STATUS_OK) return status;
    ndk::ScopedAParcel output;
    status = AIBinder_transact(remote.get(), kSetParameters, input.getR(), output.getR(), 0);
    if (status != STATUS_OK) return status;
    ndk::ScopedAStatus result;
    status = AParcel_readStatusHeader(output.get(), result.getR());
    return status == STATUS_OK ? result.getStatus() : status;
}
