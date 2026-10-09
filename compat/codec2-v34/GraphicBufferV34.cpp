// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The Android 14 android::GraphicBuffer that the stock FP6 libcodec2_vndk.so
// (sha256 f1d104621b5575f6603fa6779b55548034c2b11be85e6fcd47bffb2afc199e5d)
// constructs, at the Android 14 size.
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
// Surface) constructs GraphicBuffer objects in the stock library, and none
// leaves the library: it wraps the producer's buffers itself
// (hgbp_createFromHandle, CLONE_HANDLE) instead of going through
// AHardwareBuffer, and uses them only through inline accessors (the Android 14
// offsets checked below), initCheck, the AHardwareBuffer pointer casts and
// RefBase.
// - Hardware video decoding off (the default; ro.vendor.diamaneos.hw_video_decode
//   is not "1"): the service exposes only encoders, which write to linear
//   buffers and never use that pool. The constructors fail closed: the object
//   holds no buffer and reports NO_INIT. Every caller in C2BqBuffer.cpp checks
//   initCheck() (or the requestBuffer result built from it) and returns an
//   error, so a client that attaches a Surface to a hardware codec gets an
//   error.
// - Hardware video decoding on: the decoders render into the app's Surface
//   through that pool, so the object does what Android 14's GraphicBuffer did
//   for the two constructors the library calls. The handle constructor accepts
//   only CLONE_HANDLE, the one method the library passes: it imports a clone of
//   the producer's handle with the Android 17 mapper (which validates it
//   against the description) and the destructor frees that clone, as Android
//   14's free_handle did. Everything stays within the 256 bytes.
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
#include <unistd.h>

#include <atomic>
#include <functional>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <android-base/properties.h>
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
    // Android 14 GraphicBuffer::mOwner values.
    enum : uint8_t { kOwnNone = 0, kOwnHandle = 1, kOwnData = 2 };

    // usable: false makes the object report NO_INIT (hardware decoding off).
    explicit GraphicBufferV34(bool usable);

    status_t initCheck() const { return static_cast<status_t>(mInitCheck); }

    // Android 14 GraphicBuffer::initWithHandle for CLONE_HANDLE.
    status_t importClone(const native_handle_t* inHandle, uint32_t inWidth, uint32_t inHeight,
                         int32_t inFormat, uint32_t inLayerCount, uint64_t inUsage,
                         uint32_t inStride);

    // Android 14 data members. Public only so that the layout checks below can
    // name them; the stock library reads the ANativeWindowBuffer fields,
    // mInitCheck and mGenerationNumber through inline accessors.
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
    // Deleted through RefBase when the last strong reference goes. The library
    // never registers death callbacks (it does not import addDeathCallback), so
    // mDeathCallbacks stays empty.
    ~GraphicBufferV34() override;
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

// Android 14 GraphicBuffer::HandleWrapMethod::CLONE_HANDLE.
constexpr uint8_t kCloneHandle = 3;

// Android 14's getUniqueId(): the process id and a counter.
uint64_t nextBufferId() {
    static std::atomic<uint32_t> next{0};
    return (static_cast<uint64_t>(getpid()) << 32) |
           next.fetch_add(1, std::memory_order_relaxed);
}

GraphicBufferV34::GraphicBufferV34(bool usable)
    : BASE(),
      mOwner(kOwnData),
      mBufferMapper(GraphicBufferMapper::get()),
      mInitCheck(usable ? NO_ERROR : NO_INIT),
      mTransportNumFds(0),
      mTransportNumInts(0),
      mId(nextBufferId()),
      mBufferId(-1),
      mGenerationNumber(0) {
    width = height = stride = format = usage_deprecated = 0;
    usage = 0;
    layerCount = 0;
    handle = nullptr;
}

status_t GraphicBufferV34::importClone(const native_handle_t* inHandle, uint32_t inWidth,
                                       uint32_t inHeight, int32_t inFormat,
                                       uint32_t inLayerCount, uint64_t inUsage,
                                       uint32_t inStride) {
    buffer_handle_t imported = nullptr;
    status_t err = mBufferMapper.importBuffer(inHandle, inWidth, inHeight, inLayerCount,
                                              inFormat, inUsage, inStride, &imported);
    if (err != NO_ERROR || imported == nullptr) {
        return err != NO_ERROR ? err : NO_INIT;
    }
    width = static_cast<int>(inWidth);
    height = static_cast<int>(inHeight);
    stride = static_cast<int>(inStride);
    format = inFormat;
    usage = inUsage;
    usage_deprecated = static_cast<int>(inUsage);
    layerCount = inLayerCount;
    mOwner = kOwnHandle;
    mBufferMapper.getTransportSize(imported, &mTransportNumFds, &mTransportNumInts);
    handle = imported;
    return NO_ERROR;
}

GraphicBufferV34::~GraphicBufferV34() {
    // Only importClone sets a handle, and the object owns that clone.
    if (handle != nullptr && mOwner == kOwnHandle) {
        mBufferMapper.freeBuffer(handle);
    }
    handle = nullptr;
}

// Hardware video decoding is on for this boot: media/init.fp6.media.rc sets
// the read-only property at zygote-start, before the codec service starts.
bool surfaceOutputAllowed() {
    static const bool allowed =
            android::base::GetProperty("ro.vendor.diamaneos.hw_video_decode", "") == "1";
    return allowed;
}

std::atomic<bool> gRefusalLogged{false};

void refuse(void* self) {
    new (self) GraphicBufferV34(false);
    if (!gRefusalLogged.exchange(true, std::memory_order_relaxed)) {
        ALOGW("GraphicBuffer refused: Codec2 output to a Surface needs hardware video decoding "
              "(Android 14 ABI, fails closed)");
    }
}

}  // namespace

// Android 14: GraphicBuffer::GraphicBuffer().
// Both stock callers (C2BqBuffer.cpp) use it as an empty slot that requestBuffer
// then replaces with a handle-wrapping buffer.
void fp6GraphicBufferV34Construct(void* self) __asm__("_ZN7android13GraphicBufV34C1Ev");
void fp6GraphicBufferV34Construct(void* self) {
    if (!surfaceOutputAllowed()) {
        refuse(self);
        return;
    }
    new (self) GraphicBufferV34(true);
}

// Android 14: GraphicBuffer::GraphicBuffer(const native_handle_t*,
//     HandleWrapMethod, uint32_t width, uint32_t height, PixelFormat format,
//     uint32_t layerCount, uint64_t usage, uint32_t stride).
// Both stock callers pass CLONE_HANDLE, which never takes ownership of the
// caller's handle: the object imports and owns a clone. Any other method, or
// no handle, gives an object that reports NO_INIT.
void fp6GraphicBufferV34ConstructWithHandle(void* self, const native_handle_t* inHandle,
                                            uint8_t method, uint32_t inWidth, uint32_t inHeight,
                                            int32_t inFormat, uint32_t inLayerCount,
                                            uint64_t inUsage, uint32_t inStride)
        __asm__("_ZN7android13GraphicBufV34C1EPK13native_handleNS0_16HandleWrapMethodEjjijmj");
void fp6GraphicBufferV34ConstructWithHandle(void* self, const native_handle_t* inHandle,
                                            uint8_t method, uint32_t inWidth, uint32_t inHeight,
                                            int32_t inFormat, uint32_t inLayerCount,
                                            uint64_t inUsage, uint32_t inStride) {
    if (!surfaceOutputAllowed()) {
        refuse(self);
        return;
    }
    auto* buffer = new (self) GraphicBufferV34(true);
    status_t err = NO_INIT;
    if (method == kCloneHandle && inHandle != nullptr) {
        err = buffer->importClone(inHandle, inWidth, inHeight, inFormat, inLayerCount, inUsage,
                                  inStride);
    } else {
        ALOGW("GraphicBuffer refused: handle wrap method %u", static_cast<unsigned>(method));
    }
    buffer->mInitCheck = err;
}

// Android 14: status_t GraphicBuffer::initCheck() const.
status_t fp6GraphicBufferV34InitCheck(const void* self) __asm__("_ZNK7android13GraphicBufV349initCheckEv");
status_t fp6GraphicBufferV34InitCheck(const void* self) {
    return static_cast<const GraphicBufferV34*>(self)->initCheck();
}

// Android 14: GraphicBuffer::fromAHardwareBuffer and toAHardwareBuffer. An
// AHardwareBuffer is a GraphicBuffer, so these are pointer identities (as in
// Android 14 and 17 libui). The stock library uses them only between its own
// objects (hgbp_b2h and hgbp_describe).
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
