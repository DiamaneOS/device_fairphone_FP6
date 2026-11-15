// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The parts of libc2hwjail_avservices (c2hwjail.cpp) that the host test
// (tests/c2hwjail_test.cpp) runs on their own.

#pragma once

#include <sys/stat.h>
#include <sys/types.h>

#include <functional>
#include <set>
#include <string>
#include <unordered_set>

struct sock_fprog;

namespace c2hwjail {

// What the configuration check reads, so the host test can replace it.
struct Platform {
    // A system property, "" if unset or unreadable.
    std::function<std::string(const char* name)> getProperty;
    // A whole file. False if it cannot be read.
    std::function<bool(const std::string& path, std::string* content)> readFile;
    // stat(2): 0, or the errno.
    std::function<int(const char* path, struct stat* st)> statPath;
};

// The device's properties, files and nodes.
Platform DevicePlatform();

// The Qualcomm codecs the store may offer this boot: the five hardware
// encoders, plus the three non-secure hardware decoders when hardware video
// decoding is for all video or for selected apps. If the mode, the target
// variant, the target specification the Qualcomm library reads or the decoder
// node differ from what this boot expects, none, and *reason says why. Never
// aborts. For selected apps, *callerChecked (if given) gets the three
// decoders, which the store offers only to the callers SelectedAppsCheck
// allows. Empty in every other state.
std::set<std::string> AllowedCodecs(const Platform& platform, std::string* reason,
                                    std::set<std::string>* callerChecked = nullptr);

// What a store client does with a decoder, for the selected-apps check.
enum class Use { kList, kInterface, kComponent };

// Whether a caller may use the hardware decoders when they are for selected
// apps. A component (the decoder that gets the video) only for an app UID the
// user selected: diamaneos.hw_video_decode.uid.<uid> is "1" (system_server
// sets it, sepolicy/system-ext-private/hw_video_decode.te). The list and an
// interface also for media (1013), which builds the system codec list. Every
// other UID, and anything unexpected, is no.
bool MayUseDecoders(uid_t uid, Use use, const Platform& platform);

// The check FilterStore runs on each call. The device passes the binder
// caller's UID (c2hwjail.cpp). The host test passes its own.
using CallerCheck = std::function<bool(Use use)>;
CallerCheck SelectedAppsCheck(Platform platform, std::function<uid_t()> callingUid);

// Removes everything but the allowed codecs from the set the Qualcomm codec
// plugin registers from. The plugin registers every codec it knows when the
// set is empty, so the result is never empty: with nothing left it holds only
// a name no codec has.
void KeepAllowedCodecs(std::unordered_set<std::string>* codecs,
                       const std::set<std::string>& allowed);

// Rewrites every "trap" result of a compiled filter to "fail with EPERM", so a
// call outside the policy fails instead of killing the service. Returns the
// number of instructions changed.
size_t TrapsToErrno(struct sock_fprog* prog);

// Compiles the minijail policy at path and installs it for every thread of
// the process, with no_new_privs. Calls outside it fail with EPERM (logged by
// the kernel audit as type 1326 records). With logOnly they are logged and
// allowed. False, and *error, if it cannot.
bool InstallFilter(const char* path, bool logOnly, std::string* error);

}  // namespace c2hwjail
