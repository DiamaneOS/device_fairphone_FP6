// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Seccomp filter and codec gate for the stock Qualcomm Codec2 video service
// (vendor.qti.media.c2@1.0-service).
//
// The tools renderer renames the service's libavservices_minijail.so
// dependency to this library (same name length, pinned input and output
// hashes). This library links libavservices_minijail and the service still
// links libcodec2_hidl@1.2, but it comes before both in the service's
// dependency order, so it defines the two functions the service's main()
// calls through them:
//
// - android::SetUpMinijail (main(), before it opens binder or loads the
//   Qualcomm store): installs DiamaneOS's filter (codec2-service.policy)
//   for every thread, then the stock policy on top through the real
//   libavservices_minijail.
// - the constructor of the Codec2 HIDL ComponentStore (main(), with the
//   Qualcomm store it created, before it registers the service): installs the
//   filter if it is not in place yet, checks the codec configuration
//   (c2hwjail_check.cpp) and builds the real HIDL store around a filtering
//   store (c2hwjail_store.cpp) that offers only the codecs this boot allows:
//   the hardware encoders, the three non-secure hardware decoders when
//   hardware video decoding is on, and none if the check fails or the filter
//   could not be installed.
//
// So the service always registers, and every Codec2 client (MediaCodec,
// MediaCodecList, system_server's checks at boot) gets an answer: a store
// with the allowed codecs or an empty one. Nothing here runs from an ELF
// constructor, nothing aborts on a failed check, and calls outside the
// policy fail with EPERM instead of killing the service. The Qualcomm
// library registering every codec when it cannot read its target
// specification no longer reaches any client: only the filtering store's
// list does.

#define LOG_TAG "c2hwjail"

#include <dlfcn.h>
#include <unistd.h>

#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <C2Component.h>
#include <log/log.h>

#include "c2hwjail.h"
#include "c2hwjail_store.h"

namespace {

constexpr char kPolicy[] = "/vendor/etc/seccomp_policy/codec2-service.policy";
#ifdef C2HWJAIL_LOG_ONLY
constexpr bool kLogOnly = true;
#else
constexpr bool kLogOnly = false;
#endif

constexpr char kMinijailLibrary[] = "libavservices_minijail.so";
constexpr char kSetUpMinijail[] =
        "_ZN7android13SetUpMinijailERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_"
        "9allocatorIcEEEES8_";
constexpr char kHidlLibrary[] = "libcodec2_hidl@1.2.so";
#define C2HWJAIL_STORE_CTOR \
    "_ZN7android8hardware5media2c24V1_25utils14ComponentStoreC1ERKNSt3__110shared_ptrI16C2ComponentStoreEE"

std::once_flag gFilterOnce;
bool gFilterInstalled = false;

void EnsureFilter() {
    std::call_once(gFilterOnce, [] {
        std::string detail;
        gFilterInstalled = c2hwjail::InstallFilter(kPolicy, kLogOnly, &detail);
        if (gFilterInstalled) {
            ALOGI("seccomp filter installed (%s, %s)", kLogOnly ? "log-only" : "EPERM",
                  detail.c_str());
        } else {
            ALOGE("seccomp filter not installed (%s): no hardware codec will be offered",
                  detail.c_str());
        }
    });
}

// The real definition in a library the service already loaded.
void* RealSymbol(const char* library, const char* symbol) {
    void* handle = dlopen(library, RTLD_NOW | RTLD_NOLOAD);
    if (handle == nullptr) {
        ALOGE("%s is not loaded: %s", library, dlerror());
        return nullptr;
    }
    void* address = dlsym(handle, symbol);
    if (address == nullptr) {
        ALOGE("%s has no %s: %s", library, symbol, dlerror());
    }
    dlclose(handle);
    return address;
}

}  // namespace

namespace android {

// The stock service calls this before it opens binder or loads the store.
void SetUpMinijail(const std::string& base_policy_path, const std::string& additional_policy_path) {
    EnsureFilter();
    // The real one aborts when it cannot read the base policy; that would
    // stop the service from registering, so DiamaneOS's filter stays alone.
    if (access(base_policy_path.c_str(), R_OK) != 0) {
        ALOGE("stock seccomp policy %s unreadable, not stacked", base_policy_path.c_str());
        return;
    }
    using Fn = void (*)(const std::string&, const std::string&);
    auto real = reinterpret_cast<Fn>(RealSymbol(kMinijailLibrary, kSetUpMinijail));
    if (real == nullptr) {
        ALOGE("stock seccomp policy not stacked");
        return;
    }
    real(base_policy_path, additional_policy_path);
}

}  // namespace android

// android::hardware::media::c2::V1_2::utils::ComponentStore::ComponentStore(
//     const std::shared_ptr<C2ComponentStore>&), complete-object constructor.
// On AArch64 it takes `this` in x0 and returns void, as declared here.
void c2hwjailStoreCtor(void* self, const std::shared_ptr<C2ComponentStore>& store)
        __asm__(C2HWJAIL_STORE_CTOR);
void c2hwjailStoreCtor(void* self, const std::shared_ptr<C2ComponentStore>& store) {
    EnsureFilter();
    std::set<std::string> allowed;
    std::string reason;
    if (!gFilterInstalled) {
        reason = "no seccomp filter";
    } else if (store == nullptr) {
        reason = "no Qualcomm store";
    } else {
        allowed = c2hwjail::AllowedCodecs(c2hwjail::DevicePlatform(), &reason);
    }
    std::shared_ptr<C2ComponentStore> filtered = c2hwjail::FilterStore(store, allowed);
    const size_t offered = filtered->listComponents().size();
    if (allowed.empty()) {
        ALOGE("codec configuration differs (%s): the store offers no hardware codec",
              reason.c_str());
    } else {
        ALOGI("codec configuration checked (%s): the store offers %zu components", reason.c_str(),
              offered);
    }
    using Ctor = void (*)(void*, const std::shared_ptr<C2ComponentStore>&);
    auto real = reinterpret_cast<Ctor>(RealSymbol(kHidlLibrary, C2HWJAIL_STORE_CTOR));
    // The service links libcodec2_hidl@1.2 for this very constructor, so the
    // lookup cannot fail on a booting system; without it there is no store
    // to build at all.
    LOG_ALWAYS_FATAL_IF(real == nullptr, "cannot build the Codec2 store");
    real(self, filtered);
}
