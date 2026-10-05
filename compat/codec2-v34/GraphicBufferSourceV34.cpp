// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Android 14 GraphicBufferSource::getHGraphicBufferProducer() const for the
// stock FP6 libcodec2_hidl@1.0.so, @1.1.so and @1.2.so (sha256 f77c0f4a...,
// 8c637bfd..., 9d87cc30...). Android 17 libstagefright_bufferqueue_helper no
// longer has it (it keeps getHGraphicBufferProducer_V1_0 and
// getIGraphicBufferProducer).
//
// Its only caller is ComponentStore::createInputSurface in each of the three
// libraries (disassembly), the store-side persistent input surface. The
// framework asks a store for one only when debug.stagefright.c2inputsurface is
// above 0; this product sets -1 (media/media.mk), so the framework builds the
// input surface in the app process instead and never calls it. For any other
// caller the store answers with an input surface that has no producer, and
// that caller fails; nothing in the service dereferences the producer.
//
// Defined under the Android 14 mangled name (the symbol the stock libraries
// import). On AArch64 a const member function takes `this` in x0 and returns
// an sp<> (not trivially copyable) through the result pointer in x8, exactly
// as this free function with the same parameter and return type does.

#define LOG_TAG "bqhelper_v34compat"

#include <android/hardware/graphics/bufferqueue/2.0/IGraphicBufferProducer.h>
#include <log/log.h>
#include <utils/StrongPointer.h>

namespace android {
class GraphicBufferSource;
}  // namespace android

using HGraphicBufferProducer = ::android::hardware::graphics::bufferqueue::V2_0::IGraphicBufferProducer;

android::sp<HGraphicBufferProducer> fp6GetHGraphicBufferProducerV34(const android::GraphicBufferSource* self)
        __asm__("_ZNK7android19GraphicBufferSource25getHGraphicBufferProducerEv");

android::sp<HGraphicBufferProducer> fp6GetHGraphicBufferProducerV34(const android::GraphicBufferSource*) {
    ALOGE("Codec2 store input surfaces are not supported (debug.stagefright.c2inputsurface=-1)");
    return nullptr;
}

// This library must load the real helper: the stock libraries reach every
// other GraphicBufferSource symbol through this library's dependency on it.
// Referencing one Android 17 helper symbol keeps that dependency even if the
// linker drops unused ones, and makes the link fail if the helper changes
// again.
extern "C" void fp6BufferQueueHelperAnchor()
        __asm__("_ZNK7android19GraphicBufferSource30getHGraphicBufferProducer_V1_0Ev");
__attribute__((used)) static void (*const kBufferQueueHelperAnchor)() = &fp6BufferQueueHelperAnchor;
