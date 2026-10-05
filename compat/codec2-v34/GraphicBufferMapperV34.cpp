// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Android 14 GraphicBufferMapper::lock and ::unlock for the stock FP6
// libcodec2_vndk.so (sha256 f1d104621b5575f6603fa6779b55548034c2b11be85e6fcd47bffb2afc199e5d).
// It calls them from C2AllocationGralloc::map, ::unmap and the destructor
// (CPU access to graphic blocks). Android 17 libui keeps the same operations
// under new signatures:
//   lock(buffer_handle_t, uint32_t usage, const Rect&, void**)
//   unlock(buffer_handle_t, base::unique_fd* outFence)
// Each function below is defined under the Android 14 mangled name (the
// symbol the stock library imports) and forwards to the Android 17 member.
// On AArch64 a member function takes `this` in x0, as the first parameter
// here does.

#define LOG_TAG "uiv34"

#include <android-base/unique_fd.h>
#include <android/sync.h>
#include <log/log.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>

using android::GraphicBufferMapper;
using android::Rect;
using android::status_t;

// Android 14:
//   status_t lock(buffer_handle_t handle, uint32_t usage, const Rect& bounds, void** vaddr,
//                 int32_t* outBytesPerPixel = nullptr, int32_t* outBytesPerStride = nullptr);
// A synchronous lock (lockAsync with no acquire fence), which is what the
// Android 17 four-argument lock does. Gralloc 4 and later do not report
// bytes per pixel or stride; Android 14 already returned -1 (unknown) there.
status_t fp6GraphicBufferMapperLockV34(GraphicBufferMapper* self, buffer_handle_t handle,
                                       uint32_t usage, const Rect& bounds, void** vaddr,
                                       int32_t* outBytesPerPixel, int32_t* outBytesPerStride)
        __asm__("_ZN7android19GraphicBufferMapper4lockEPK13native_handlejRKNS_4RectEPPvPiS9_");

status_t fp6GraphicBufferMapperLockV34(GraphicBufferMapper* self, buffer_handle_t handle,
                                       uint32_t usage, const Rect& bounds, void** vaddr,
                                       int32_t* outBytesPerPixel, int32_t* outBytesPerStride) {
    if (outBytesPerPixel) *outBytesPerPixel = -1;
    if (outBytesPerStride) *outBytesPerStride = -1;
    return self->lock(handle, usage, bounds, vaddr);
}

// Android 14:
//   status_t unlock(buffer_handle_t handle);
// It waited for the release fence before returning. Android 17 hands the fence
// to the caller; the wait is done here so callers keep the old guarantee that
// the CPU is done with the buffer when unlock returns.
status_t fp6GraphicBufferMapperUnlockV34(GraphicBufferMapper* self, buffer_handle_t handle)
        __asm__("_ZN7android19GraphicBufferMapper6unlockEPK13native_handle");

status_t fp6GraphicBufferMapperUnlockV34(GraphicBufferMapper* self, buffer_handle_t handle) {
    android::base::unique_fd fence;
    status_t error = self->unlock(handle, &fence);
    if (error == android::NO_ERROR && fence.ok() && sync_wait(fence.get(), -1) != 0) {
        ALOGW("unlock: waiting for the release fence failed");
    }
    return error;
}
