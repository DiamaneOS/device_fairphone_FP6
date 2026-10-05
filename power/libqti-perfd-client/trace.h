// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Debug tracing for the perf client stand-in, off unless enabled with
//   adb shell setprop log.tag.perfd-client-stub D

#pragma once

#define LOG_TAG "perfd-client-stub"

#include <android/log.h>

static inline int tracing(void) {
    return __android_log_is_loggable(ANDROID_LOG_DEBUG, LOG_TAG, ANDROID_LOG_INFO);
}

#define TRACE(...)                                                      \
    do {                                                                \
        if (tracing()) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__); \
    } while (0)
