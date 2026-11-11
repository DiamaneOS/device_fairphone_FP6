// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Calls uicam's entry points under the renamed names the stock binaries use,
// on 256-byte blocks as the stock binaries allocate them.

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <android/hardware_buffer.h>
#include <cutils/native_handle.h>
#include <gtest/gtest.h>
#include <nativebase/nativebase.h>
#include <system/graphics.h>
#include <vndk/hardware_buffer.h>

using status_t = int32_t;

void ctorHandle(void*, const native_handle_t*, uint8_t, uint32_t, uint32_t, int32_t, uint32_t,
                uint64_t, uint32_t)
        __asm__("_ZN7android13GraphicBufCamC1EPK13native_handleNS0_16HandleWrapMethodEjjijmj");
void ctorAlloc(void*, uint32_t, uint32_t, int32_t, uint32_t, std::string)
        __asm__("_ZN7android13GraphicBufCamC1EjjijNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
status_t lockAsync(void*, uint32_t, void**, int, int32_t*, int32_t*)
        __asm__("_ZN7android13GraphicBufCam9lockAsyncEjPPviPiS3_");
status_t lockAsyncYCbCr(void*, uint32_t, android_ycbcr*, int)
        __asm__("_ZN7android13GraphicBufCam14lockAsyncYCbCrEjP13android_ycbcri");
status_t lockYCbCr(void*, uint32_t, android_ycbcr*)
        __asm__("_ZN7android13GraphicBufCam9lockYCbCrEjP13android_ycbcr");
status_t unlockBuffer(void*) __asm__("_ZN7android13GraphicBufCam6unlockEv");
status_t initCheck(const void*) __asm__("_ZNK7android13GraphicBufCam9initCheckEv");
AHardwareBuffer* toAHardwareBuffer(void*) __asm__("_ZN7android13GraphicBufCam17toAHardwareBufferEv");
void* fromAHardwareBuffer(AHardwareBuffer*)
        __asm__("_ZN7android13GraphicBufCam19fromAHardwareBufferEP15AHardwareBuffer");

namespace {

constexpr uint32_t kCpuOften = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                               AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;

// The Android 14 layout the stock code reads inline: the ANativeWindowBuffer
// at offset 16, mInitCheck at 200.
ANativeWindowBuffer* native(void* object) {
    return reinterpret_cast<ANativeWindowBuffer*>(static_cast<char*>(object) + 16);
}

// A 256-byte block as the stock code allocates it (operator new). RefBase
// deletes the object when the last reference goes. An object that wrote past
// the 256 bytes corrupts the allocator's canary, and the free aborts the test.
struct Block {
    Block() : memory(::operator new(256)) {}
    void* get() { return memory; }
    void* memory;
};

// Drops the last strong reference, as sp<GraphicBuffer> does, through the
// ANativeWindowBuffer's decRef hook (ANativeObjectBase).
void release(void* object) {
    ANativeWindowBuffer* buffer = native(object);
    buffer->common.decRef(&buffer->common);
}

void acquire(void* object) {
    ANativeWindowBuffer* buffer = native(object);
    buffer->common.incRef(&buffer->common);
}

}  // namespace

TEST(UicamTest, AllocatedBlobLocksAndStaysInside) {
    Block block;
    ctorAlloc(block.get(), 4096, 1, HAL_PIXEL_FORMAT_BLOB, kCpuOften, "test");
    acquire(block.get());
    ASSERT_EQ(0, initCheck(block.get()));
    EXPECT_EQ(4096, native(block.get())->width);
    EXPECT_EQ(HAL_PIXEL_FORMAT_BLOB, native(block.get())->format);
    EXPECT_NE(nullptr, native(block.get())->handle);
    void* address = nullptr;
    int32_t bpp = 0, bps = 0;
    ASSERT_EQ(0, lockAsync(block.get(), kCpuOften, &address, -1, &bpp, &bps));
    ASSERT_NE(nullptr, address);
    // Android 14's mapper reported neither value.
    EXPECT_EQ(-1, bpp);
    EXPECT_EQ(-1, bps);
    memset(address, 0x5a, 4096);
    EXPECT_EQ(0, unlockBuffer(block.get()));
    release(block.get());
}

// The app hands the service its capture metadata in a BLOB buffer, which the
// service wraps (CLONE_HANDLE) and locks with lockAsync.
TEST(UicamTest, WrappedBlobLocks) {
    AHardwareBuffer_Desc desc = {};
    desc.width = 65536;
    desc.height = 1;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_BLOB;
    desc.usage = kCpuOften;
    AHardwareBuffer* source = nullptr;
    ASSERT_EQ(0, AHardwareBuffer_allocate(&desc, &source));
    AHardwareBuffer_describe(source, &desc);
    Block wrapped;
    ctorHandle(wrapped.get(), AHardwareBuffer_getNativeHandle(source), 3 /* CLONE_HANDLE */,
               desc.width, desc.height, static_cast<int32_t>(desc.format), desc.layers, desc.usage,
               desc.stride);
    acquire(wrapped.get());
    ASSERT_EQ(0, initCheck(wrapped.get()));
    void* address = nullptr;
    int32_t bpp = 0, bps = 0;
    ASSERT_EQ(0, lockAsync(wrapped.get(), kCpuOften, &address, -1, &bpp, &bps));
    ASSERT_NE(nullptr, address);
    memset(address, 0x5a, desc.width);
    EXPECT_EQ(0, unlockBuffer(wrapped.get()));
    release(wrapped.get());
    AHardwareBuffer_release(source);
}

TEST(UicamTest, Nv21LocksAsYCbCr) {
    Block block;
    ctorAlloc(block.get(), 640, 480, HAL_PIXEL_FORMAT_YCrCb_420_SP,
              kCpuOften | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, "test");
    acquire(block.get());
    ASSERT_EQ(0, initCheck(block.get()));
    android_ycbcr ycbcr = {};
    ASSERT_EQ(0, lockYCbCr(block.get(), kCpuOften, &ycbcr));
    EXPECT_NE(nullptr, ycbcr.y);
    EXPECT_NE(nullptr, ycbcr.cb);
    EXPECT_NE(nullptr, ycbcr.cr);
    EXPECT_EQ(2u, ycbcr.chroma_step);
    EXPECT_EQ(0, unlockBuffer(block.get()));
    ASSERT_EQ(0, lockAsyncYCbCr(block.get(), kCpuOften, &ycbcr, -1));
    EXPECT_EQ(0, unlockBuffer(block.get()));
    release(block.get());
}

TEST(UicamTest, CloneAndTakeHandleWrapAnotherBuffer) {
    AHardwareBuffer_Desc desc = {};
    desc.width = 320;
    desc.height = 240;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
    desc.usage = kCpuOften;
    AHardwareBuffer* source = nullptr;
    ASSERT_EQ(0, AHardwareBuffer_allocate(&desc, &source));
    AHardwareBuffer_describe(source, &desc);
    const native_handle_t* sourceHandle = AHardwareBuffer_getNativeHandle(source);

    Block cloned;
    ctorHandle(cloned.get(), sourceHandle, 3 /* CLONE_HANDLE */, desc.width, desc.height,
               static_cast<int32_t>(desc.format), desc.layers, desc.usage, desc.stride);
    acquire(cloned.get());
    ASSERT_EQ(0, initCheck(cloned.get()));
    android_ycbcr ycbcr = {};
    ASSERT_EQ(0, lockAsyncYCbCr(cloned.get(), kCpuOften, &ycbcr, -1));
    EXPECT_EQ(0, unlockBuffer(cloned.get()));
    AHardwareBuffer* backing = toAHardwareBuffer(cloned.get());
    ASSERT_NE(nullptr, backing);
    EXPECT_NE(static_cast<void*>(backing), cloned.get());
    AHardwareBuffer_Desc backingDesc = {};
    AHardwareBuffer_describe(backing, &backingDesc);
    EXPECT_EQ(desc.width, backingDesc.width);

    // TAKE_HANDLE gets its own copy of the handle and takes it over.
    native_handle_t* taken = native_handle_clone(sourceHandle);
    ASSERT_NE(nullptr, taken);
    Block took;
    ctorHandle(took.get(), taken, 1 /* TAKE_HANDLE */, desc.width, desc.height,
               static_cast<int32_t>(desc.format), desc.layers, desc.usage, desc.stride);
    acquire(took.get());
    ASSERT_EQ(0, initCheck(took.get()));
    void* address = nullptr;
    ASSERT_EQ(0, lockAsync(took.get(), kCpuOften, &address, -1, nullptr, nullptr));
    EXPECT_EQ(0, unlockBuffer(took.get()));

    release(cloned.get());
    release(took.get());
    AHardwareBuffer_release(source);
}

TEST(UicamTest, RefusedWrapMethodsAndBadAllocationsFailClosed) {
    Block wrapped;
    native_handle_t* handle = native_handle_create(0, 0);
    ctorHandle(wrapped.get(), handle, 0 /* WRAP_HANDLE */, 16, 16, HAL_PIXEL_FORMAT_RGBA_8888, 1,
               kCpuOften, 16);
    acquire(wrapped.get());
    EXPECT_NE(0, initCheck(wrapped.get()));
    void* address = nullptr;
    EXPECT_NE(0, lockAsync(wrapped.get(), kCpuOften, &address, -1, nullptr, nullptr));
    EXPECT_NE(0, unlockBuffer(wrapped.get()));
    EXPECT_EQ(nullptr, toAHardwareBuffer(wrapped.get()));
    release(wrapped.get());
    native_handle_delete(handle);

    Block bad;
    ctorAlloc(bad.get(), 0, 0, HAL_PIXEL_FORMAT_RGBA_8888, kCpuOften, "test");
    acquire(bad.get());
    EXPECT_NE(0, initCheck(bad.get()));
    android_ycbcr ycbcr = {};
    EXPECT_NE(0, lockYCbCr(bad.get(), kCpuOften, &ycbcr));
    release(bad.get());
}

TEST(UicamTest, ForeignAndroid17BuffersAreUsedThroughTheNdk) {
    AHardwareBuffer_Desc desc = {};
    desc.width = 1024;
    desc.height = 1;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_BLOB;
    desc.usage = kCpuOften;
    AHardwareBuffer* foreign = nullptr;
    ASSERT_EQ(0, AHardwareBuffer_allocate(&desc, &foreign));
    void* object = fromAHardwareBuffer(foreign);
    EXPECT_EQ(static_cast<void*>(foreign), object);
    EXPECT_EQ(0, initCheck(object));
    void* address = nullptr;
    ASSERT_EQ(0, lockAsync(object, kCpuOften, &address, -1, nullptr, nullptr));
    EXPECT_NE(nullptr, address);
    EXPECT_EQ(0, unlockBuffer(object));
    EXPECT_EQ(foreign, toAHardwareBuffer(object));
    AHardwareBuffer_release(foreign);
}
