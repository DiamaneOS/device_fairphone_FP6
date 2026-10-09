// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Configuration check and seccomp loader for the stock Qualcomm Codec2 video
// service (vendor.qti.media.c2@1.0-service).
//
// The tools renderer renames the service's libavservices_minijail.so
// dependency to this library (same name length, pinned input and output
// hashes), and this library links libavservices_minijail in turn, so the
// service's own SetUpMinijail call still installs the stock policy on top of
// ours. The dynamic linker runs the constructor below after the service's
// dependencies are loaded and before its main(), so before the Qualcomm store
// reads its configuration:
//
// 1. Fail closed on the codec configuration. libqcodec2_platform registers
//    every codec, decoders and secure codecs included, when its target
//    specification cannot be read (vendor.media.target_variant unreadable, the
//    file missing or not parsable). The constructor reads the same property
//    and file in the same process and domain and requires exactly the codecs
//    this boot allows: the five hardware encoders, plus the three non-secure
//    hardware decoders when hardware video decoding is on
//    (ro.vendor.diamaneos.hw_video_decode=1, media/init.fp6.media.rc). It also
//    requires the decoder node's owner and mode for that state. Any mismatch
//    aborts the service, so it never registers a codec list it did not check.
// 2. Install DiamaneOS's own seccomp filter (codec2-service.policy) for the
//    whole process, whether or not the stock service keeps calling
//    SetUpMinijail. If the policy cannot be read, compiled or installed, the
//    service aborts: it never runs unconfined.
//
// Calls outside the policy, and listed calls whose arguments do not match their
// rule, are
// - logged and allowed (SECCOMP_RET_LOG, kernel audit record type 1326) when
//   built with C2HWJAIL_LOG_ONLY, to collect a profile;
// - otherwise trapped: SIGSYS, and the service dies (init restarts it).
// Nothing is executed and the SELinux domain does not change (mediacodec).

#define LOG_TAG "c2hwjail"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>

#include <set>
#include <string>

#include <android-base/file.h>
#include <android-base/properties.h>
#include <cutils/android_filesystem_config.h>
#include <json/reader.h>
#include <json/value.h>
#include <log/log.h>

// Internal libminijail headers, linked statically from the same source tree.
// minijail's public API cannot make RET_LOG the default action on Android
// (it never detects RET_LOG there), so the policy is compiled directly.
#include "syscall_filter.h"
#include "syscall_wrapper.h"

namespace {

constexpr char kPolicy[] = "/vendor/etc/seccomp_policy/codec2-service.policy";

#ifdef C2HWJAIL_LOG_ONLY
constexpr char kFilterMode[] = "log-only";
constexpr block_action kFilterAction = ACTION_RET_LOG;
#else
constexpr char kFilterMode[] = "trap";
constexpr block_action kFilterAction = ACTION_RET_TRAP;
#endif

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

// Built on first use: the constructor below can run before this file's dynamic initialisers, so
// namespace-scope std::set objects would still be unconstructed (null tree) when it reads them.
const std::set<std::string>& Encoders() {
    static const std::set<std::string> names = {
            "c2.qti.avc.encoder",      "c2.qti.hevc.encoder", "c2.qti.hevc.encoder.cq",
            "c2.qti.hevc.encoder.hdr", "c2.qti.heic.encoder",
    };
    return names;
}
// No secure (DRM) and no low-latency variant.
const std::set<std::string>& Decoders() {
    static const std::set<std::string> names = {
            "c2.qti.avc.decoder",
            "c2.qti.hevc.decoder",
            "c2.qti.vp9.decoder",
    };
    return names;
}

// Adds the strings of a codec list to names; a missing list counts as empty.
void AddCodecNames(const Json::Value& list, const char* what, std::set<std::string>* names) {
    if (list.isNull()) {
        return;
    }
    if (!list.isArray()) {
        LOG_ALWAYS_FATAL("target specification: %s is not a list", what);
    }
    for (Json::ArrayIndex i = 0; i < list.size(); ++i) {
        if (!list[i].isString()) {
            LOG_ALWAYS_FATAL("target specification: %s has an entry that is not a name", what);
        }
        names->insert(list[i].asString());
    }
}

void CheckCodecConfiguration() {
    const std::string mode = android::base::GetProperty(kModeProperty, "");
    if (!mode.empty() && mode != "0" && mode != "1") {
        LOG_ALWAYS_FATAL("%s is \"%s\"", kModeProperty, mode.c_str());
    }
    const bool decoders = mode == "1";
    const char* expected = decoders ? kDecodersVariant : kEncodersOnlyVariant;
    const std::string variant = android::base::GetProperty(kVariantProperty, "");
    if (variant != expected) {
        LOG_ALWAYS_FATAL("%s is \"%s\", expected \"%s\"", kVariantProperty, variant.c_str(),
                         expected);
    }

    // The same path libqcodec2_platform builds from the property.
    const std::string path = "/vendor/etc/media" + variant + "/video_system_specs.json";
    std::string text;
    if (!android::base::ReadFileToString(path, &text, /*follow_symlinks=*/false)) {
        LOG_ALWAYS_FATAL("cannot read %s: %s", path.c_str(), strerror(errno));
    }
    if (text.size() > kMaxSpecBytes) {
        LOG_ALWAYS_FATAL("%s is too large", path.c_str());
    }
    // libqcodec2_platform parses it with this jsoncpp reader (Json::Reader,
    // all features: comments allowed, no trailing commas), so a file this
    // check accepts is one the library reads the same way.
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(text, root, /*collectComments=*/false) || !root.isObject()) {
        LOG_ALWAYS_FATAL("cannot parse %s", path.c_str());
    }
    const Json::Value& video = root["Video"];
    if (!video.isObject()) {
        LOG_ALWAYS_FATAL("%s has no Video object", path.c_str());
    }
    // libqcodec2_platform merges these three lists into the one set the
    // codec plugin registers; an empty set would register every codec.
    std::set<std::string> listed;
    const Json::Value& available = video["codecs-available"];
    if (!available.isObject()) {
        LOG_ALWAYS_FATAL("%s has no codecs-available object", path.c_str());
    }
    AddCodecNames(available["decoders"], "codecs-available.decoders", &listed);
    AddCodecNames(available["encoders"], "codecs-available.encoders", &listed);
    AddCodecNames(video["OptionalCodecs"], "OptionalCodecs", &listed);
    std::set<std::string> allowed = Encoders();
    if (decoders) {
        allowed.insert(Decoders().begin(), Decoders().end());
    }
    if (listed != allowed) {
        LOG_ALWAYS_FATAL("%s lists %zu codecs, not the %zu this boot allows", path.c_str(),
                         listed.size(), allowed.size());
    }

    // The decoder node: root-only while hardware decoding is off; root and
    // group mediacodec, read-write, while it is on. A missing node (video
    // driver not loaded) cannot be opened at all.
    struct stat node;
    if (stat(kDecoderNode, &node) != 0) {
        if (errno != ENOENT) {
            LOG_ALWAYS_FATAL("cannot stat %s: %s", kDecoderNode, strerror(errno));
        }
        ALOGW("%s does not exist", kDecoderNode);
    } else {
        const gid_t group = decoders ? AID_MEDIA_CODEC : AID_ROOT;
        const mode_t access = decoders ? 0660 : 0600;
        if (!S_ISCHR(node.st_mode) || node.st_uid != AID_ROOT || node.st_gid != group ||
            (node.st_mode & 07777) != access) {
            LOG_ALWAYS_FATAL("%s is %u:%u %04o, expected 0:%u %04o", kDecoderNode,
                             static_cast<unsigned>(node.st_uid), static_cast<unsigned>(node.st_gid),
                             static_cast<unsigned>(node.st_mode & 07777),
                             static_cast<unsigned>(group), static_cast<unsigned>(access));
        }
    }
    ALOGI("codec configuration checked: hardware encoders%s", decoders ? " and decoders" : " only");
}

void InstallFilter() {
    const struct filter_options options = {
            .action = kFilterAction,
            // RET_LOG needs allow_logging, which also makes the compiler skip
            // unknown system call names instead of failing (log-only builds).
            .allow_logging = kFilterAction == ACTION_RET_LOG,
            .allow_syscalls_for_logging = 0,
            .allow_duplicate_syscalls = false,
            .include_libc_compatibility_allowlist = false,
    };
    struct sock_fprog prog = {};

    FILE* policy = fopen(kPolicy, "re");
    if (policy == nullptr) {
        LOG_ALWAYS_FATAL("cannot open %s: %s", kPolicy, strerror(errno));
    }
    int compiled = compile_filter(kPolicy, policy, &prog, &options);
    fclose(policy);
    if (compiled != 0) {
        LOG_ALWAYS_FATAL("cannot compile %s", kPolicy);
    }

    // The service holds only CAP_SYS_NICE, so the kernel requires no_new_privs.
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        LOG_ALWAYS_FATAL("cannot set no_new_privs: %s", strerror(errno));
    }
    // TSYNC applies the filter and no_new_privs to every thread of the process;
    // a positive result names a thread that could not be synchronised.
    int installed = sys_seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &prog);
    if (installed != 0) {
        LOG_ALWAYS_FATAL("cannot install the seccomp filter: %s",
                         installed > 0 ? "a thread could not be synchronised" : strerror(errno));
    }
    ALOGI("seccomp filter installed (%s, %u instructions)", kFilterMode,
          static_cast<unsigned>(prog.len));
    free(prog.filter);
}

}  // namespace

// The first log line also opens the log socket before the filter is in place.
__attribute__((constructor)) static void c2hwjail_start() {
    ALOGI("checking the codec configuration");
    CheckCodecConfiguration();
    InstallFilter();
}
