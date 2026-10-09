// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The parts of libc2hwjail_avservices (c2hwjail.cpp) that the host test
// (tests/c2hwjail_test.cpp) runs on their own.

#pragma once

#include <sys/stat.h>

#include <functional>
#include <set>
#include <string>

struct sock_fprog;

namespace c2hwjail {

// What the configuration check reads, so the host test can replace it.
struct Platform {
    // A system property, "" if unset or unreadable.
    std::function<std::string(const char* name)> getProperty;
    // A whole file; false if it cannot be read.
    std::function<bool(const std::string& path, std::string* content)> readFile;
    // stat(2): 0, or the errno.
    std::function<int(const char* path, struct stat* st)> statPath;
};

// The device's properties, files and nodes.
Platform DevicePlatform();

// The Qualcomm codecs the store may offer this boot: the five hardware
// encoders, plus the three non-secure hardware decoders when hardware video
// decoding is on. If the mode, the target variant, the target specification
// the Qualcomm library reads or the decoder node differ from what this boot
// expects, none, and *reason says why. Never aborts.
std::set<std::string> AllowedCodecs(const Platform& platform, std::string* reason);

// Rewrites every "trap" result of a compiled filter to "fail with EPERM", so a
// call outside the policy fails instead of killing the service. Returns the
// number of instructions changed.
size_t TrapsToErrno(struct sock_fprog* prog);

// Compiles the minijail policy at path and installs it for every thread of
// the process, with no_new_privs. Calls outside it fail with EPERM (logged by
// the kernel audit as type 1326 records); with logOnly they are logged and
// allowed. False, and *error, if it cannot.
bool InstallFilter(const char* path, bool logOnly, std::string* error);

}  // namespace c2hwjail
