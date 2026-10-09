// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The codec configuration check and the seccomp filter installation of
// libc2hwjail_avservices (c2hwjail.cpp). No namespace-scope object here needs
// a constructor, and nothing aborts: every failure is a result.

#include "c2hwjail.h"

#include <errno.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>

#include <iterator>

#include <android-base/file.h>
#include <android-base/properties.h>
#include <cutils/android_filesystem_config.h>
#include <json/reader.h>
#include <json/value.h>

// Internal libminijail headers, linked statically from the same source tree.
// minijail's public API cannot make RET_LOG the default action on Android
// (it never detects RET_LOG there), so the policy is compiled directly.
#include "syscall_filter.h"
#include "syscall_wrapper.h"

// Older host kernel headers (the host test) lack these.
#ifndef SECCOMP_RET_ACTION_FULL
#define SECCOMP_RET_ACTION_FULL 0xffff0000U
#endif
#ifndef SECCOMP_FILTER_FLAG_TSYNC
#define SECCOMP_FILTER_FLAG_TSYNC 1
#endif

namespace c2hwjail {
namespace {

// Set once per boot by media/init.fp6.media.rc from the Settings switch.
constexpr char kModeProperty[] = "ro.vendor.diamaneos.hw_video_decode";
// Read by libqcodec2_platform (qc2::TargetSpec::readTargetVariant), which
// opens /vendor/etc/media<variant>/video_system_specs.json.
constexpr char kVariantProperty[] = "vendor.media.target_variant";
constexpr char kEncodersOnlyVariant[] = "_volcano_v1";
constexpr char kDecodersVariant[] = "_volcano_v1_hwdec";
// msm_vidc registers the Iris decoder at minor 32 (boot/ueventd.rc).
constexpr char kDecoderNode[] = "/dev/video32";
// The target specifications are a few kilobytes.
constexpr size_t kMaxSpecBytes = 1 << 20;

// Plain arrays: constant-initialized, so usable at any time.
constexpr const char* kEncoders[] = {
        "c2.qti.avc.encoder",      "c2.qti.hevc.encoder", "c2.qti.hevc.encoder.cq",
        "c2.qti.hevc.encoder.hdr", "c2.qti.heic.encoder",
};
// No secure (DRM) and no low-latency variant.
constexpr const char* kDecoders[] = {
        "c2.qti.avc.decoder",
        "c2.qti.hevc.decoder",
        "c2.qti.vp9.decoder",
};

// Adds the names of a codec list; a missing list counts as empty.
bool AddCodecNames(const Json::Value& list, const char* what, std::set<std::string>* names,
                   std::string* reason) {
    if (list.isNull()) {
        return true;
    }
    if (!list.isArray()) {
        *reason = std::string("target specification: ") + what + " is not a list";
        return false;
    }
    for (Json::ArrayIndex i = 0; i < list.size(); ++i) {
        if (!list[i].isString()) {
            *reason = std::string("target specification: ") + what + " has an entry that is not a name";
            return false;
        }
        names->insert(list[i].asString());
    }
    return true;
}

}  // namespace

Platform DevicePlatform() {
    Platform platform;
    platform.getProperty = [](const char* name) { return android::base::GetProperty(name, ""); };
    platform.readFile = [](const std::string& path, std::string* content) {
        return android::base::ReadFileToString(path, content, /*follow_symlinks=*/false);
    };
    platform.statPath = [](const char* path, struct stat* st) {
        return stat(path, st) == 0 ? 0 : errno;
    };
    return platform;
}

std::set<std::string> AllowedCodecs(const Platform& platform, std::string* reason) {
    const std::string mode = platform.getProperty(kModeProperty);
    if (!mode.empty() && mode != "0" && mode != "1") {
        *reason = std::string(kModeProperty) + " is \"" + mode + "\"";
        return {};
    }
    const bool decoders = mode == "1";
    const std::string expected = decoders ? kDecodersVariant : kEncodersOnlyVariant;
    const std::string variant = platform.getProperty(kVariantProperty);
    if (variant != expected) {
        *reason = std::string(kVariantProperty) + " is \"" + variant + "\", expected \"" + expected +
                  "\"";
        return {};
    }

    // The same path libqcodec2_platform builds from the property.
    const std::string path = "/vendor/etc/media" + variant + "/video_system_specs.json";
    std::string text;
    if (!platform.readFile(path, &text)) {
        *reason = "cannot read " + path;
        return {};
    }
    if (text.size() > kMaxSpecBytes) {
        *reason = path + " is too large";
        return {};
    }
    // libqcodec2_platform parses it with this jsoncpp reader (Json::Reader,
    // all features: comments allowed, no trailing commas), so a file this
    // check accepts is one the library reads the same way.
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(text, root, /*collectComments=*/false) || !root.isObject()) {
        *reason = "cannot parse " + path;
        return {};
    }
    const Json::Value& video = root["Video"];
    if (!video.isObject()) {
        *reason = path + " has no Video object";
        return {};
    }
    const Json::Value& available = video["codecs-available"];
    if (!available.isObject()) {
        *reason = path + " has no codecs-available object";
        return {};
    }
    // libqcodec2_platform merges these three lists into the one set the codec
    // plugin registers; an empty set would register every codec.
    std::set<std::string> listed;
    if (!AddCodecNames(available["decoders"], "codecs-available.decoders", &listed, reason) ||
        !AddCodecNames(available["encoders"], "codecs-available.encoders", &listed, reason) ||
        !AddCodecNames(video["OptionalCodecs"], "OptionalCodecs", &listed, reason)) {
        return {};
    }
    std::set<std::string> allowed(std::begin(kEncoders), std::end(kEncoders));
    if (decoders) {
        allowed.insert(std::begin(kDecoders), std::end(kDecoders));
    }
    if (listed != allowed) {
        *reason = path + " lists " + std::to_string(listed.size()) + " codecs, not the " +
                  std::to_string(allowed.size()) + " this boot allows";
        return {};
    }

    // The decoder node: root-only while hardware decoding is off; root and
    // group mediacodec, read-write, while it is on. A missing node (video
    // driver not loaded) cannot be opened at all.
    struct stat node = {};
    const int error = platform.statPath(kDecoderNode, &node);
    if (error != 0 && error != ENOENT) {
        *reason = std::string("cannot stat ") + kDecoderNode + ": " + strerror(error);
        return {};
    }
    if (error == 0) {
        const gid_t group = decoders ? AID_MEDIA_CODEC : AID_ROOT;
        const mode_t access = decoders ? 0660 : 0600;
        if (!S_ISCHR(node.st_mode) || node.st_uid != AID_ROOT || node.st_gid != group ||
            (node.st_mode & 07777) != access) {
            char found[160];
            snprintf(found, sizeof(found), "%s is %u:%u %04o, expected 0:%u %04o", kDecoderNode,
                     static_cast<unsigned>(node.st_uid), static_cast<unsigned>(node.st_gid),
                     static_cast<unsigned>(node.st_mode & 07777), static_cast<unsigned>(group),
                     static_cast<unsigned>(access));
            *reason = found;
            return {};
        }
    }
    *reason = decoders ? "hardware encoders and decoders" : "hardware encoders only";
    return allowed;
}

size_t TrapsToErrno(struct sock_fprog* prog) {
    size_t changed = 0;
    for (unsigned short i = 0; i < prog->len; ++i) {
        struct sock_filter* insn = &prog->filter[i];
        if (insn->code == (BPF_RET | BPF_K) &&
            (insn->k & SECCOMP_RET_ACTION_FULL) == SECCOMP_RET_TRAP) {
            insn->k = SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA);
            ++changed;
        }
    }
    return changed;
}

bool InstallFilter(const char* path, bool logOnly, std::string* error) {
    const struct filter_options options = {
            .action = logOnly ? ACTION_RET_LOG : ACTION_RET_TRAP,
            // RET_LOG needs allow_logging, which also makes the compiler skip
            // unknown system call names instead of failing (log-only builds).
            .allow_logging = logOnly ? 1 : 0,
            .allow_syscalls_for_logging = 0,
            .allow_duplicate_syscalls = false,
            .include_libc_compatibility_allowlist = false,
    };
    struct sock_fprog prog = {};

    FILE* policy = fopen(path, "re");
    if (policy == nullptr) {
        *error = std::string("cannot open ") + path + ": " + strerror(errno);
        return false;
    }
    const int compiled = compile_filter(path, policy, &prog, &options);
    fclose(policy);
    if (compiled != 0) {
        *error = std::string("cannot compile ") + path;
        return false;
    }
    if (!logOnly && TrapsToErrno(&prog) == 0) {
        free(prog.filter);
        *error = "the compiled filter has no trap result";
        return false;
    }

    // The service holds only CAP_SYS_NICE, so the kernel requires no_new_privs.
    bool installed = false;
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        *error = std::string("cannot set no_new_privs: ") + strerror(errno);
    } else {
        // TSYNC applies the filter and no_new_privs to every thread of the
        // process; a positive result names a thread that could not be
        // synchronised.
        const int result = sys_seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &prog);
        if (result != 0) {
            *error = result > 0 ? "a thread could not be synchronised"
                                : std::string("cannot install the filter: ") + strerror(errno);
        } else {
            *error = std::to_string(prog.len) + " instructions";
            installed = true;
        }
    }
    free(prog.filter);
    return installed;
}

}  // namespace c2hwjail
