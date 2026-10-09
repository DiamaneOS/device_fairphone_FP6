// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <sys/stat.h>

#include <memory>
#include <set>
#include <string>

// The Codec2 headers use bionic's __unused, which glibc lacks (the host
// test); glibc's own headers name struct fields __unused, so it is defined
// only around the Codec2 headers, after the system headers.
#if !defined(__BIONIC__) && !defined(__unused)
#define __unused __attribute__((__unused__))
#define C2HWJAIL_HOST_UNUSED
#endif
#include <C2Component.h>
#ifdef C2HWJAIL_HOST_UNUSED
#undef __unused
#undef C2HWJAIL_HOST_UNUSED
#endif

namespace c2hwjail {

// A C2ComponentStore that offers only the allowed components of store: it
// lists and creates those (and their aliases) and nothing else, and forwards
// the store-level calls. With nothing allowed it is an empty store. The
// Codec2 core header C2Component.h is identical in the Android 14 build the
// stock HIDL library and Qualcomm store use and in Android 17.
std::shared_ptr<C2ComponentStore> FilterStore(std::shared_ptr<C2ComponentStore> store,
                                              std::set<std::string> allowed);

}  // namespace c2hwjail
