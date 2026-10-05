// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The stock camera's perf hints that reach the power HAL (camera_hints.cpp).

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Internal to the library: its exports stay the Qualcomm client API.
#define CAMERA_HINTS_INTERNAL __attribute__((visibility("hidden")))

// Starts or renews a camera hint. Returns the request's handle when the hint
// becomes a power HAL boost, or 0 when it does not (not a forwarded hint, the
// request table is full or the forwarding thread could not start); the
// caller then answers as the no-op stub. Renewing one of these handles with a
// hint that is not forwarded ends its boost. Never blocks on binder.
CAMERA_HINTS_INTERNAL int camera_hint_acquire(int handle, int hint, int duration_ms);

// Ends the boost a handle from camera_hint_acquire started. Returns 1 if the
// handle was one, 0 otherwise. Never blocks on binder.
CAMERA_HINTS_INTERNAL int camera_hint_release(int handle);

#ifdef __cplusplus
}
#endif
