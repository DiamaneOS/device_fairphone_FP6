// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Stand-in for Qualcomm's perf client. Exports the calls the stock vendor code
// looks up with dlsym:
//   camera.qcom.so:       perf_lock_acq, perf_lock_rel, perf_hint, perf_hint_renew
//   libsdmextension.so:   perf_lock_acq, perf_lock_rel, perf_hint_acq_rel_offload,
//                         perf_lock_rel_offload
//   source composer (cpuhint.cpp, only with ro.vendor.extension_library set):
//                         perf_hint_acq_rel_offload, perf_lock_rel_offload,
//                         perf_hint_offload, perf_event
// plus the rest of the client API LineageOS's stub provides. The camera's
// open, close and snapshot hints (perf_hint, perf_hint_renew) become
// time-limited power HAL boosts, and perf_lock_rel ends them
// (camera_hints.cpp). Every other request succeeds without doing anything:
// CPU, GPU and scheduler boosts come from the power HAL (powerhint.json). To
// see the requests and the boosts they start:
//   adb shell setprop log.tag.perfd-client-stub D

#include "camera_hints.h"
#include "trace.h"

// A positive handle: callers treat zero or negative handles as failure.
#define STUB_HANDLE 1

static int handle_or_stub(int handle) {
    return handle > 0 ? handle : STUB_HANDLE;
}

int perf_lock_acq(int handle, int duration, int list[], int numArgs) {
    TRACE("perf_lock_acq handle=%d duration=%d args=%d", handle, duration, numArgs);
    return handle_or_stub(handle);
}

int perf_lock_rel(int handle) {
    int ended = camera_hint_release(handle);
    TRACE("perf_lock_rel handle=%d%s", handle, ended ? " (ends its camera boost)" : "");
    return 0;
}

int perf_lock_acq_rel(int handle, int duration, int list[], int numArgs, int reserveNumArgs) {
    TRACE("perf_lock_acq_rel handle=%d duration=%d args=%d", handle, duration, numArgs);
    return handle_or_stub(handle);
}

int perf_lock_use_profile(int handle, int profile) {
    TRACE("perf_lock_use_profile handle=%d profile=%d", handle, profile);
    return handle_or_stub(handle);
}

void perf_lock_cmd(int cmd) {
    TRACE("perf_lock_cmd cmd=%d", cmd);
}

int perf_hint(int hint, const char* pkg, int duration, int type) {
    int forwarded = camera_hint_acquire(0, hint, duration);
    int result = forwarded > 0 ? forwarded : STUB_HANDLE;
    TRACE("perf_hint hint=0x%x duration=%d type=%d -> %d", hint, duration, type, result);
    return result;
}

int perf_hint_renew(int handle, int hint, const char* pkg, int duration, int type, int numArgs,
                    int list[]) {
    int forwarded = camera_hint_acquire(handle, hint, duration);
    int result = forwarded > 0 ? forwarded : handle_or_stub(handle);
    TRACE("perf_hint_renew handle=%d hint=0x%x duration=%d type=%d -> %d", handle, hint, duration,
          type, result);
    return result;
}

int perf_hint_acq_rel(int handle, int hint, const char* pkg, int duration, int type, int numArgs,
                      int list[]) {
    TRACE("perf_hint_acq_rel handle=%d hint=0x%x duration=%d type=%d", handle, hint, duration,
          type);
    return handle_or_stub(handle);
}

int perf_hint_acq_rel_offload(int handle, int hint, const char* pkg, int duration, int type,
                              int numArgs, int list[]) {
    TRACE("perf_hint_acq_rel_offload handle=%d hint=0x%x duration=%d type=%d", handle, hint,
          duration, type);
    return handle_or_stub(handle);
}

int perf_hint_offload(int hint, const char* pkg, int duration, int type, int numArgs, int list[]) {
    TRACE("perf_hint_offload hint=0x%x duration=%d type=%d", hint, duration, type);
    return STUB_HANDLE;
}

int perf_lock_rel_offload(int handle) {
    TRACE("perf_lock_rel_offload handle=%d", handle);
    return 0;
}

void perf_event(int event, const char* pkg, int numArgs, int list[]) {
    TRACE("perf_event event=0x%x args=%d", event, numArgs);
}

int perf_get_feedback(int request, const char* pkg) {
    TRACE("perf_get_feedback request=0x%x", request);
    return -1;
}

int perf_get_feedback_extn(int request, const char* pkg, int numArgs, int list[]) {
    TRACE("perf_get_feedback_extn request=0x%x", request);
    return -1;
}
