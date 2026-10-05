// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The Android 14 android::GraphicBuffer that the stock FP6 libcodec2_vndk.so
// (sha256 f1d104621b5575f6603fa6779b55548034c2b11be85e6fcd47bffb2afc199e5d)
// constructs, as a fail-closed object of the Android 14 size.
//
// The stock library was built against Android 14, where GraphicBuffer is 256
// bytes. At each of its four construction sites it allocates 256 bytes itself
// (operator new) and then calls the GraphicBuffer constructor. Android 17's
// GraphicBuffer is 3376 bytes, so the Android 17 constructor would write far
// past that allocation. The tools renderer therefore renames the six
// GraphicBuffer symbols the stock library imports from android::GraphicBuffer
// to android::GraphicBufV34 (same length; input and output hashes pinned), and
// its libui.so dependency to uiv34.so. Only this library defines those names,
// so the stock calls cannot bind to Android 17 libui, whatever the load order,
// and nothing else in the process can bind to this class.
//
// Only the platform bufferqueue block pool (C2BqBuffer.cpp, codec output to a
// Surface) constructs GraphicBuffer objects in the stock library. The service
// exposes only encoders, which write to linear buffers and never use that
// pool. So the constructors fail closed: the object has the Android 14 layout
// and size, holds no buffer and reports NO_INIT. Every caller in
// C2BqBuffer.cpp checks initCheck() (or the requestBuffer result built from
// it) and returns an error, so a client that attaches a Surface to a hardware
// codec gets an error instead of a heap overflow in the codec service.
//
// The exported functions below are defined under the renamed mangled names.
// On AArch64 a complete-object constructor takes `this` in x0 and returns
// void, and a member function takes `this` in x0, as the first parameter of
// each function here does. libuiv34.map.txt exports these and the two
// GraphicBufferMapper entry points and nothing else.

#define LOG_TAG "uiv34"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <atomic>
#include <functional>
#include <new>
#include <utility>
#include <vector>

#include <android/hardware_buffer.h>
#include <cutils/native_handle.h>
#include <log/log.h>
#include <nativebase/nativebase.h>
#include <ui/ANativeObjectBase.h>
#include <ui/GraphicBufferMapper.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>

namespace android {
namespace {

// The Android 14 GraphicBuffer layout: the same bases and data members, in the
// same order, as ui/GraphicBuffer.h at the stock pin. Its third base,
// Flattenable<GraphicBuffer>, is empty and is left out. Android 17 keeps these
// members at the same offsets and appends a 3120-byte mDependencyMonitor.
class GraphicBufferV34 final
    : public ANativeObjectBase<ANativeWindowBuffer, GraphicBufferV34, RefBase> {
  public:
    GraphicBufferV34();

    status_t initCheck() const { return static_cast<status_t>(mInitCheck); }

    // Android 14 data members. Public only so that the layout checks below can
    // name them; nothing here reads them except mInitCheck.
    uint8_t mOwner;
    GraphicBufferMapper& mBufferMapper;
    ssize_t mInitCheck;
    uint32_t mTransportNumFds;
    uint32_t mTransportNumInts;
    uint64_t mId;
    int32_t mBufferId;
    uint32_t mGenerationNumber;
    std::vector<std::pair<std::function<void(void*, uint64_t)>, void*>> mDeathCallbacks;

  private:
    // Deleted through RefBase when the last strong reference goes. Nothing to
    // free: the object never holds a handle (mOwner is Android 14 ownNone).
    ~GraphicBufferV34() override = default;
};

// The Android 14 offsets, as the Android 14 libui constructor and the stock
// library's inlined accessors use them (RefBase first at 0, then the
// ANativeWindowBuffer at 16, then GraphicBuffer's own members).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(sizeof(GraphicBufferV34) == 256, "the stock library allocates 256 bytes");
static_assert(alignof(GraphicBufferV34) <= alignof(max_align_t), "operator new alignment");
static_assert(offsetof(GraphicBufferV34, common) == 16, "ANativeWindowBuffer base");
static_assert(offsetof(GraphicBufferV34, width) == 72, "ANativeWindowBuffer::width");
static_assert(offsetof(GraphicBufferV34, layerCount) == 96, "ANativeWindowBuffer::layerCount");
static_assert(offsetof(GraphicBufferV34, handle) == 112, "ANativeWindowBuffer::handle");
static_assert(offsetof(GraphicBufferV34, usage) == 120, "ANativeWindowBuffer::usage");
static_assert(offsetof(GraphicBufferV34, mOwner) == 184, "GraphicBuffer::mOwner");
static_assert(offsetof(GraphicBufferV34, mBufferMapper) == 192, "GraphicBuffer::mBufferMapper");
static_assert(offsetof(GraphicBufferV34, mInitCheck) == 200, "GraphicBuffer::mInitCheck");
static_assert(offsetof(GraphicBufferV34, mId) == 216, "GraphicBuffer::mId");
static_assert(offsetof(GraphicBufferV34, mBufferId) == 224, "GraphicBuffer::mBufferId");
static_assert(offsetof(GraphicBufferV34, mGenerationNumber) == 228, "GraphicBuffer::mGenerationNumber");
static_assert(offsetof(GraphicBufferV34, mDeathCallbacks) == 232, "GraphicBuffer::mDeathCallbacks");
#pragma clang diagnostic pop

GraphicBufferV34::GraphicBufferV34()
    : BASE(),
      mOwner(0),
      mBufferMapper(GraphicBufferMapper::get()),
      mInitCheck(NO_INIT),
      mTransportNumFds(0),
      mTransportNumInts(0),
      mId(0),
      mBufferId(-1),
      mGenerationNumber(0) {
    width = height = stride = format = usage_deprecated = 0;
    usage = 0;
    layerCount = 0;
    handle = nullptr;
}

std::atomic<bool> gRefusalLogged{false};

void construct(void* self) {
    new (self) GraphicBufferV34();
    if (!gRefusalLogged.exchange(true, std::memory_order_relaxed)) {
        ALOGW("GraphicBuffer refused: Codec2 output to a Surface is not supported by this "
              "service (Android 14 ABI, fails closed)");
    }
}

}  // namespace

// Android 14: GraphicBuffer::GraphicBuffer().
// Both stock callers (C2BqBuffer.cpp) use it as an empty slot that requestBuffer
// then replaces with a handle-wrapping buffer.
void fp6GraphicBufferV34Construct(void* self) __asm__("_ZN7android13GraphicBufV34C1Ev");
void fp6GraphicBufferV34Construct(void* self) {
    construct(self);
}

// Android 14: GraphicBuffer::GraphicBuffer(const native_handle_t*,
//     HandleWrapMethod, uint32_t width, uint32_t height, PixelFormat format,
//     uint32_t layerCount, uint64_t usage, uint32_t stride).
// Both stock callers pass CLONE_HANDLE, which never takes ownership of the
// handle, so the handle stays with the caller and is not touched here.
void fp6GraphicBufferV34ConstructWithHandle(void* self, const native_handle_t* inHandle,
                                            uint8_t method, uint32_t inWidth, uint32_t inHeight,
                                            int32_t inFormat, uint32_t inLayerCount,
                                            uint64_t inUsage, uint32_t inStride)
        __asm__("_ZN7android13GraphicBufV34C1EPK13native_handleNS0_16HandleWrapMethodEjjijmj");
void fp6GraphicBufferV34ConstructWithHandle(void* self, const native_handle_t* /*inHandle*/,
                                            uint8_t /*method*/, uint32_t /*inWidth*/,
                                            uint32_t /*inHeight*/, int32_t /*inFormat*/,
                                            uint32_t /*inLayerCount*/, uint64_t /*inUsage*/,
                                            uint32_t /*inStride*/) {
    construct(self);
}

// Android 14: status_t GraphicBuffer::initCheck() const.
// The stock library also calls it on Android 17 GraphicBuffers that it wraps
// from an AHardwareBuffer (bufferqueue h2b); mInitCheck is at the same offset
// there.
status_t fp6GraphicBufferV34InitCheck(const void* self) __asm__("_ZNK7android13GraphicBufV349initCheckEv");
status_t fp6GraphicBufferV34InitCheck(const void* self) {
    return static_cast<const GraphicBufferV34*>(self)->initCheck();
}

// Android 14: GraphicBuffer::fromAHardwareBuffer and toAHardwareBuffer. An
// AHardwareBuffer is a GraphicBuffer, so these are pointer identities (as in
// Android 14 and 17 libui).
void* fp6GraphicBufferV34FromAHardwareBuffer(AHardwareBuffer* buffer)
        __asm__("_ZN7android13GraphicBufV3419fromAHardwareBufferEP15AHardwareBuffer");
void* fp6GraphicBufferV34FromAHardwareBuffer(AHardwareBuffer* buffer) {
    return buffer;
}

const void* fp6GraphicBufferV34FromConstAHardwareBuffer(const AHardwareBuffer* buffer)
        __asm__("_ZN7android13GraphicBufV3419fromAHardwareBufferEPK15AHardwareBuffer");
const void* fp6GraphicBufferV34FromConstAHardwareBuffer(const AHardwareBuffer* buffer) {
    return buffer;
}

AHardwareBuffer* fp6GraphicBufferV34ToAHardwareBuffer(void* self)
        __asm__("_ZN7android13GraphicBufV3417toAHardwareBufferEv");
AHardwareBuffer* fp6GraphicBufferV34ToAHardwareBuffer(void* self) {
    return static_cast<AHardwareBuffer*>(self);
}

}  // namespace android
