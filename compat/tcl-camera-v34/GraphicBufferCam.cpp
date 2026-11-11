// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The Android 14 android::GraphicBuffer that Fairphone's camera algorithm
// stack (the TCL "algoservice" and its libraries, built against Android 14)
// constructs, at the Android 14 size.
//
// The stock binaries allocate 256 bytes themselves (operator new) and then
// call the GraphicBuffer constructor. Android 17's GraphicBuffer is 3376 bytes
// (a 3120-byte mDependencyMonitor at offset 256), so its constructor would
// write far past that allocation. The tools renderer renames the
// GraphicBuffer symbols the three stock binaries import to
// android::GraphicBufCam (same length, input and output hashes pinned) and
// their libui.so dependency to uicam.so. Only this library defines those
// names, so the stock calls cannot bind to Android 17 libui, and nothing else
// in the process can bind to this class:
// - vendor/bin/hw/cameraalgoservice: the handle constructor (CLONE_HANDLE and
//   TAKE_HANDLE), lockAsync, lockAsyncYCbCr, unlock and toAHardwareBuffer.
// - vendor/lib64/libtctcameraalgo.so: the allocating constructor (BLOB
//   buffers), lockAsync, unlock, initCheck and fromAHardwareBuffer.
// - vendor/lib64/com.tct.cameraalgo.distortion.so: the allocating constructor
//   (NV21 buffers), lockYCbCr and unlock.
//
// Every object is backed by an AHardwareBuffer from the NDK interface
// (libnativewindow, an LLNDK library that uses the platform's own libui in
// its own namespace). The object keeps the Android 14 fields the stock code
// reads through inline accessors (the ANativeWindowBuffer members, mInitCheck,
// mId, mGenerationNumber) and nothing else. Locking and unlocking go through
// the AHardwareBuffer, and toAHardwareBuffer hands out that AHardwareBuffer,
// a real Android 17 object, never this 256-byte one. So no Android 17 code
// ever treats this object as its own GraphicBuffer.
//
// Unlike uiv34 (codec and distortion node), this library uses no libui code
// at all: the consumers import nothing else from libui.
//
// The exported functions below are defined under the renamed mangled names.
// On AArch64 a complete-object constructor takes `this` in x0 and returns
// void, and a member function takes `this` in x0, as the first parameter of
// each function here does. libuicam.map.txt exports these and nothing else.

#define LOG_TAG "uicam"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <android/hardware_buffer.h>
#include <cutils/native_handle.h>
#include <log/log.h>
#include <nativebase/nativebase.h>
#include <system/graphics.h>
#include <ui/ANativeObjectBase.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <vndk/hardware_buffer.h>

namespace android {
namespace {

// The Android 14 GraphicBuffer layout: the same bases and data members, in the
// same order, as ui/GraphicBuffer.h of Android 14. Its third base,
// Flattenable<GraphicBuffer>, is empty and is left out.
class GraphicBufferCam final
    : public ANativeObjectBase<ANativeWindowBuffer, GraphicBufferCam, RefBase> {
  public:
    GraphicBufferCam();

    // Android 14 data members. Public only so that the layout checks below can
    // name them.
    uint8_t mOwner;
    // Android 14 has a GraphicBufferMapper reference here. It stays null: the
    // stock code reaches the mapper only through the functions below.
    void* mBufferMapper;
    ssize_t mInitCheck;
    uint32_t mTransportNumFds;
    uint32_t mTransportNumInts;
    uint64_t mId;
    int32_t mBufferId;
    uint32_t mGenerationNumber;
    std::vector<std::pair<std::function<void(void*, uint64_t)>, void*>> mDeathCallbacks;

    // Takes over `buffer` (one reference) and copies its description.
    void adopt(AHardwareBuffer* buffer);

  private:
    // Deleted through RefBase when the last strong reference goes.
    ~GraphicBufferCam() override;
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(sizeof(GraphicBufferCam) == 256, "the stock binaries allocate 256 bytes");
static_assert(alignof(GraphicBufferCam) <= alignof(max_align_t), "operator new alignment");
static_assert(offsetof(GraphicBufferCam, common) == 16, "ANativeWindowBuffer base");
static_assert(offsetof(GraphicBufferCam, width) == 72, "ANativeWindowBuffer::width");
static_assert(offsetof(GraphicBufferCam, height) == 76, "ANativeWindowBuffer::height");
static_assert(offsetof(GraphicBufferCam, stride) == 80, "ANativeWindowBuffer::stride");
static_assert(offsetof(GraphicBufferCam, format) == 84, "ANativeWindowBuffer::format");
static_assert(offsetof(GraphicBufferCam, layerCount) == 96, "ANativeWindowBuffer::layerCount");
static_assert(offsetof(GraphicBufferCam, handle) == 112, "ANativeWindowBuffer::handle");
static_assert(offsetof(GraphicBufferCam, usage) == 120, "ANativeWindowBuffer::usage");
static_assert(offsetof(GraphicBufferCam, mOwner) == 184, "GraphicBuffer::mOwner");
static_assert(offsetof(GraphicBufferCam, mBufferMapper) == 192, "GraphicBuffer::mBufferMapper");
static_assert(offsetof(GraphicBufferCam, mInitCheck) == 200, "GraphicBuffer::mInitCheck");
static_assert(offsetof(GraphicBufferCam, mId) == 216, "GraphicBuffer::mId");
static_assert(offsetof(GraphicBufferCam, mBufferId) == 224, "GraphicBuffer::mBufferId");
static_assert(offsetof(GraphicBufferCam, mGenerationNumber) == 228, "GraphicBuffer::mGenerationNumber");
static_assert(offsetof(GraphicBufferCam, mDeathCallbacks) == 232, "GraphicBuffer::mDeathCallbacks");
#pragma clang diagnostic pop

// Android 14 GraphicBuffer::HandleWrapMethod values.
constexpr uint8_t kTakeHandle = 1;
constexpr uint8_t kCloneHandle = 3;
// Android 14 GraphicBuffer::mOwner: ownData.
constexpr uint8_t kOwnData = 2;

// Android 14's getUniqueId(): the process id and a counter.
uint64_t nextBufferId() {
    static std::atomic<uint32_t> next{0};
    return (static_cast<uint64_t>(getpid()) << 32) |
           next.fetch_add(1, std::memory_order_relaxed);
}

// Every live object of this class and its AHardwareBuffer (null when the
// construction failed). The object has no room for the pointer.
std::mutex gObjectsLock;
std::unordered_map<const void*, AHardwareBuffer*> gObjects;

// What a member function works on.
enum class Kind { kOwn, kFailed, kForeign };

// An object of this class (with its AHardwareBuffer, or failed), or an
// Android 17 GraphicBuffer the stock code got from fromAHardwareBuffer. An
// AHardwareBuffer is a GraphicBuffer in libui, so a foreign object is used as
// the AHardwareBuffer it is.
Kind lookup(const void* self, AHardwareBuffer** buffer) {
    std::lock_guard<std::mutex> lock(gObjectsLock);
    auto found = gObjects.find(self);
    if (found == gObjects.end()) {
        *buffer = static_cast<AHardwareBuffer*>(const_cast<void*>(self));
        return Kind::kForeign;
    }
    *buffer = found->second;
    return found->second != nullptr ? Kind::kOwn : Kind::kFailed;
}

GraphicBufferCam::GraphicBufferCam()
    : BASE(),
      mOwner(kOwnData),
      mBufferMapper(nullptr),
      mInitCheck(NO_INIT),
      mTransportNumFds(0),
      mTransportNumInts(0),
      mId(nextBufferId()),
      mBufferId(-1),
      mGenerationNumber(0) {
    width = height = stride = format = usage_deprecated = 0;
    usage = 0;
    layerCount = 0;
    handle = nullptr;
    std::lock_guard<std::mutex> lock(gObjectsLock);
    gObjects[this] = nullptr;
}

void GraphicBufferCam::adopt(AHardwareBuffer* buffer) {
    AHardwareBuffer_Desc actual = {};
    AHardwareBuffer_describe(buffer, &actual);
    width = static_cast<int>(actual.width);
    height = static_cast<int>(actual.height);
    stride = static_cast<int>(actual.stride);
    format = static_cast<int>(actual.format);
    usage = actual.usage;
    usage_deprecated = static_cast<int>(actual.usage);
    layerCount = actual.layers;
    // The handle belongs to the AHardwareBuffer and lives as long as it does.
    handle = AHardwareBuffer_getNativeHandle(buffer);
    mInitCheck = handle != nullptr ? NO_ERROR : NO_INIT;
    std::lock_guard<std::mutex> lock(gObjectsLock);
    gObjects[this] = buffer;
}

GraphicBufferCam::~GraphicBufferCam() {
    handle = nullptr;
    AHardwareBuffer* buffer = nullptr;
    {
        std::lock_guard<std::mutex> lock(gObjectsLock);
        auto found = gObjects.find(this);
        if (found != gObjects.end()) {
            buffer = found->second;
            gObjects.erase(found);
        }
    }
    if (buffer != nullptr) {
        AHardwareBuffer_release(buffer);
    }
}

// Only the CPU access bits of a usage are valid for locking.
uint64_t cpuUsage(uint32_t inUsage) {
    return static_cast<uint64_t>(inUsage) &
           (AHARDWAREBUFFER_USAGE_CPU_READ_MASK | AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK);
}

// A lock on an object without a buffer: Android 14 took the fence, so close it.
status_t refuseLock(int fenceFd) {
    if (fenceFd >= 0) {
        close(fenceFd);
    }
    return NO_INIT;
}

status_t lockPlanes(const void* self, uint32_t inUsage, android_ycbcr* ycbcr, int fenceFd) {
    AHardwareBuffer* buffer = nullptr;
    if (lookup(self, &buffer) == Kind::kFailed) {
        return refuseLock(fenceFd);
    }
    if (ycbcr == nullptr) {
        refuseLock(fenceFd);
        return BAD_VALUE;
    }
    AHardwareBuffer_Planes planes = {};
    int err = AHardwareBuffer_lockPlanes(buffer, cpuUsage(inUsage), fenceFd,
                                         nullptr /* whole buffer */, &planes);
    if (err != 0) {
        return err;
    }
    if (planes.planeCount != 3) {
        AHardwareBuffer_unlock(buffer, nullptr);
        return BAD_VALUE;
    }
    *ycbcr = {};
    ycbcr->y = planes.planes[0].data;
    ycbcr->cb = planes.planes[1].data;
    ycbcr->cr = planes.planes[2].data;
    ycbcr->ystride = planes.planes[0].rowStride;
    ycbcr->cstride = planes.planes[1].rowStride;
    ycbcr->chroma_step = planes.planes[1].pixelStride;
    return NO_ERROR;
}

std::atomic<bool> gRefusalLogged{false};

}  // namespace

// Android 14: GraphicBuffer::GraphicBuffer(const native_handle_t*,
//     HandleWrapMethod, uint32_t width, uint32_t height, PixelFormat format,
//     uint32_t layerCount, uint64_t usage, uint32_t stride).
// The algorithm service wraps the buffers the camera app sends it.
// CLONE_HANDLE: the caller keeps its handle, the object owns a clone.
// TAKE_HANDLE: the object owns the caller's handle (the service frees nothing
// afterwards), so it is closed and deleted once the clone exists, or on
// failure. WRAP_HANDLE and TAKE_UNREGISTERED_HANDLE are not used by the stock
// code and give an object that reports NO_INIT.
void fp6GraphicBufferCamConstructWithHandle(void* self, const native_handle_t* inHandle,
                                            uint8_t method, uint32_t inWidth, uint32_t inHeight,
                                            int32_t inFormat, uint32_t inLayerCount,
                                            uint64_t inUsage, uint32_t inStride)
        __asm__("_ZN7android13GraphicBufCamC1EPK13native_handleNS0_16HandleWrapMethodEjjijmj");
void fp6GraphicBufferCamConstructWithHandle(void* self, const native_handle_t* inHandle,
                                            uint8_t method, uint32_t inWidth, uint32_t inHeight,
                                            int32_t inFormat, uint32_t inLayerCount,
                                            uint64_t inUsage, uint32_t inStride) {
    auto* object = new (self) GraphicBufferCam();
    if (inHandle == nullptr || (method != kCloneHandle && method != kTakeHandle)) {
        if (!gRefusalLogged.exchange(true, std::memory_order_relaxed)) {
            ALOGW("GraphicBuffer refused: handle wrap method %u", static_cast<unsigned>(method));
        }
        return;
    }
    AHardwareBuffer_Desc description = {};
    description.width = inWidth;
    description.height = inHeight;
    description.layers = inLayerCount;
    description.format = static_cast<uint32_t>(inFormat);
    description.usage = inUsage;
    description.stride = inStride;
    AHardwareBuffer* buffer = nullptr;
    int err = AHardwareBuffer_createFromHandle(&description, inHandle,
                                               AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_CLONE,
                                               &buffer);
    if (method == kTakeHandle) {
        native_handle_close(inHandle);
        native_handle_delete(const_cast<native_handle_t*>(inHandle));
    }
    if (err != 0 || buffer == nullptr) {
        object->mInitCheck = err != 0 ? err : NO_INIT;
        return;
    }
    object->adopt(buffer);
}

// Android 14: GraphicBuffer::GraphicBuffer(uint32_t inWidth, uint32_t inHeight,
//     PixelFormat inFormat, uint32_t inUsage, std::string requestorName).
// A new one-layer buffer. A failed allocation gives an object without a buffer
// that reports the error. The by-value string is passed by reference and the
// caller destroys it, as for the Android 14 constructor. The name is not used.
void fp6GraphicBufferCamConstructAllocated(void* self, uint32_t inWidth, uint32_t inHeight,
                                           int32_t inFormat, uint32_t inUsage,
                                           std::string requestorName)
        __asm__("_ZN7android13GraphicBufCamC1EjjijNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
void fp6GraphicBufferCamConstructAllocated(void* self, uint32_t inWidth, uint32_t inHeight,
                                           int32_t inFormat, uint32_t inUsage,
                                           std::string /* requestorName */) {
    auto* object = new (self) GraphicBufferCam();
    AHardwareBuffer_Desc request = {};
    request.width = inWidth;
    request.height = inHeight;
    request.layers = 1;
    request.format = static_cast<uint32_t>(inFormat);
    request.usage = static_cast<uint64_t>(inUsage);
    AHardwareBuffer* buffer = nullptr;
    int err = AHardwareBuffer_allocate(&request, &buffer);
    if (err != 0 || buffer == nullptr) {
        object->mInitCheck = err != 0 ? err : NO_INIT;
        return;
    }
    object->adopt(buffer);
}

// Android 14: status_t GraphicBuffer::lockAsync(uint32_t inUsage, void** vaddr,
//     int fenceFd, int32_t* outBytesPerPixel, int32_t* outBytesPerStride).
// The whole buffer, as Android 14 locked it. The fence is passed on, which
// takes it over as Android 14's mapper did.
status_t fp6GraphicBufferCamLockAsync(void* self, uint32_t inUsage, void** vaddr, int fenceFd,
                                      int32_t* outBytesPerPixel, int32_t* outBytesPerStride)
        __asm__("_ZN7android13GraphicBufCam9lockAsyncEjPPviPiS3_");
status_t fp6GraphicBufferCamLockAsync(void* self, uint32_t inUsage, void** vaddr, int fenceFd,
                                      int32_t* outBytesPerPixel, int32_t* outBytesPerStride) {
    AHardwareBuffer* buffer = nullptr;
    if (lookup(self, &buffer) == Kind::kFailed) {
        return refuseLock(fenceFd);
    }
    if (vaddr == nullptr) {
        refuseLock(fenceFd);
        return BAD_VALUE;
    }
    // Android 14's Gralloc4 mapper reported -1 for both and never failed a lock
    // for want of plane information. AHardwareBuffer_lockAndGetInfo does fail
    // for BLOB buffers (no planes), which carry the app's capture metadata, so
    // the plain lock is used, as Android 14 did.
    if (outBytesPerPixel != nullptr) *outBytesPerPixel = -1;
    if (outBytesPerStride != nullptr) *outBytesPerStride = -1;
    int err = AHardwareBuffer_lock(buffer, cpuUsage(inUsage), fenceFd, nullptr /* whole buffer */,
                                   vaddr);
    if (err != 0) {
        return err;
    }
    return NO_ERROR;
}

// Android 14: status_t GraphicBuffer::lockAsyncYCbCr(uint32_t inUsage,
//     android_ycbcr* ycbcr, int fenceFd).
status_t fp6GraphicBufferCamLockAsyncYCbCr(void* self, uint32_t inUsage, android_ycbcr* ycbcr,
                                           int fenceFd)
        __asm__("_ZN7android13GraphicBufCam14lockAsyncYCbCrEjP13android_ycbcri");
status_t fp6GraphicBufferCamLockAsyncYCbCr(void* self, uint32_t inUsage, android_ycbcr* ycbcr,
                                           int fenceFd) {
    return lockPlanes(self, inUsage, ycbcr, fenceFd);
}

// Android 14: status_t GraphicBuffer::lockYCbCr(uint32_t inUsage, android_ycbcr* ycbcr).
status_t fp6GraphicBufferCamLockYCbCr(void* self, uint32_t inUsage, android_ycbcr* ycbcr)
        __asm__("_ZN7android13GraphicBufCam9lockYCbCrEjP13android_ycbcr");
status_t fp6GraphicBufferCamLockYCbCr(void* self, uint32_t inUsage, android_ycbcr* ycbcr) {
    return lockPlanes(self, inUsage, ycbcr, -1 /* no fence */);
}

// Android 14: status_t GraphicBuffer::unlock(). It waited for the release
// fence before returning. AHardwareBuffer_unlock without a fence argument does
// the same.
status_t fp6GraphicBufferCamUnlock(void* self) __asm__("_ZN7android13GraphicBufCam6unlockEv");
status_t fp6GraphicBufferCamUnlock(void* self) {
    AHardwareBuffer* buffer = nullptr;
    if (lookup(self, &buffer) == Kind::kFailed) {
        return NO_INIT;
    }
    return AHardwareBuffer_unlock(buffer, nullptr);
}

// Android 14: status_t GraphicBuffer::initCheck() const.
status_t fp6GraphicBufferCamInitCheck(const void* self) __asm__("_ZNK7android13GraphicBufCam9initCheckEv");
status_t fp6GraphicBufferCamInitCheck(const void* self) {
    // An Android 17 GraphicBuffer from fromAHardwareBuffer has mInitCheck at
    // the same offset as this class.
    return static_cast<status_t>(static_cast<const GraphicBufferCam*>(self)->mInitCheck);
}

// Android 14: AHardwareBuffer* GraphicBuffer::toAHardwareBuffer(). An object
// of this class hands out its AHardwareBuffer (null if it has none), a foreign
// one is itself.
AHardwareBuffer* fp6GraphicBufferCamToAHardwareBuffer(void* self)
        __asm__("_ZN7android13GraphicBufCam17toAHardwareBufferEv");
AHardwareBuffer* fp6GraphicBufferCamToAHardwareBuffer(void* self) {
    AHardwareBuffer* buffer = nullptr;
    lookup(self, &buffer);
    return buffer;
}

// Android 14: static GraphicBuffer* GraphicBuffer::fromAHardwareBuffer(AHardwareBuffer*).
// A pointer identity, as in libui. The result is an Android 17 GraphicBuffer,
// which the member functions above treat as foreign. The stock library only
// reads its reference count (RefBase, same layout) for a log line.
void* fp6GraphicBufferCamFromAHardwareBuffer(AHardwareBuffer* buffer)
        __asm__("_ZN7android13GraphicBufCam19fromAHardwareBufferEP15AHardwareBuffer");
void* fp6GraphicBufferCamFromAHardwareBuffer(AHardwareBuffer* buffer) {
    return buffer;
}

}  // namespace android
