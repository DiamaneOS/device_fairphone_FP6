// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Bluetooth HCI service for the FP6. It registers the stock Qualcomm HCI
// implementation (android.hardware.bluetooth@1.1-impl-qti.so) through HIDL
// passthrough, as the stock android.hardware.bluetooth@1.1-service-qti does,
// with the same SoC-name step first. Unlike the stock service it does not link
// the FM, ANT, SAR, config-store and TPI libraries, which the stock service
// links but never registers on this device, and it does not report
// connectivity-proxy (Xpan) support to the BT FM codec driver, whose node the
// Bluetooth user cannot open here. Before anything else it installs a seccomp
// filter, so the closed implementation runs under it from its first load.

#define LOG_TAG "android.hardware.bluetooth@1.1-service.fp6"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <android/hardware/bluetooth/1.0/IBluetoothHci.h>
#include <android/hardware/bluetooth/1.1/IBluetoothHci.h>
#include <cutils/properties.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/LegacySupport.h>
#include <log/log.h>

// Internal libminijail headers, linked statically from the same source tree,
// as in camera/seccomp/camxjail.c: minijail's public API cannot make RET_LOG
// the default action on Android, so the policy is compiled directly.
#include "syscall_filter.h"
#include "syscall_wrapper.h"

namespace {

constexpr char kSeccompPolicy[] = "/vendor/etc/seccomp_policy/bluetooth-hci.policy";
// SECCOMP_FILTER_FLAG_LOG from linux/seccomp.h, which cannot be included next
// to minijail's bpf.h (both define struct seccomp_data).
constexpr unsigned kSeccompFilterFlagLog = 1U << 1;

#ifdef BT_SECCOMP_LOG_ONLY
constexpr block_action kSeccompAction = ACTION_RET_LOG;
constexpr char kSeccompMode[] = "log-only";
#else
constexpr block_action kSeccompAction = ACTION_RET_TRAP;
constexpr char kSeccompMode[] = "trap";
#endif

// Installs the seccomp filter for the whole process: our code, the HIDL thread
// pool, the HCI implementation's load and every thread it starts. Calls outside
// bluetooth-hci.policy, and listed calls whose arguments do not match their
// rule, are
// - logged and allowed (SECCOMP_RET_LOG, kernel audit record type 1326) when
//   built with BT_SECCOMP_LOG_ONLY, to collect the service's profile;
// - otherwise trapped: SIGSYS, a tombstone naming the call, and the service
//   dies (init restarts it).
// The filter also asks the kernel to log trapped calls and calls the policy
// answers with an error, so dmesg shows them in both modes. Nothing is
// executed and the SELinux domain does not change. If the filter cannot be
// installed the service aborts: it never runs unconfined.
void InstallSeccompFilter() {
    const filter_options options = {
            .action = kSeccompAction,
            // RET_LOG needs allow_logging, which also makes the compiler skip
            // unknown system call names instead of failing (log-only builds).
            .allow_logging = kSeccompAction == ACTION_RET_LOG,
            .allow_syscalls_for_logging = 0,
            .allow_duplicate_syscalls = false,
            .include_libc_compatibility_allowlist = false,
    };
    sock_fprog prog = {};

    FILE* policy = fopen(kSeccompPolicy, "re");
    if (policy == nullptr) {
        LOG_ALWAYS_FATAL("Cannot open %s: %s", kSeccompPolicy, strerror(errno));
    }
    const int compiled = compile_filter(kSeccompPolicy, policy, &prog, &options);
    fclose(policy);
    if (compiled != 0) {
        LOG_ALWAYS_FATAL("Cannot compile %s", kSeccompPolicy);
    }

    // The service has no CAP_SYS_ADMIN, so the kernel requires no_new_privs.
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        LOG_ALWAYS_FATAL("Cannot set no_new_privs: %s", strerror(errno));
    }
    // TSYNC applies the filter and no_new_privs to every thread of the process;
    // a positive result names a thread that could not be synchronised.
    const int installed = sys_seccomp(SECCOMP_SET_MODE_FILTER,
                                      SECCOMP_FILTER_FLAG_TSYNC | kSeccompFilterFlagLog, &prog);
    if (installed != 0) {
        LOG_ALWAYS_FATAL("Cannot install the seccomp filter: %s",
                         installed > 0 ? "a thread could not be synchronised" : strerror(errno));
    }
    ALOGI("Seccomp filter installed (%s, %u instructions)", kSeccompMode,
          static_cast<unsigned>(prog.len));
    free(prog.filter);
}

constexpr char kSocProperty[] = "persist.vendor.qcom.bluetooth.soc";
constexpr char kBtPowerDevice[] = "/dev/btpower";

// btpower BT_CMD_GET_CHIPSET_ID (kernel include/btpower.h): copies the
// MAX_PROP_SIZE (32) byte devicetree compatible string of the Bluetooth chip.
constexpr int kGetChipsetId = 0xbfaf;
constexpr size_t kChipsetIdSize = 32;

struct SocName {
    const char* chipset;
    const char* name;
};

// The stock service's chipset-to-SoC-name table, in its order. For wcn786x
// the stock service also sets persist.vendor.qcom.bluetooth.sec_soc; the FP6
// has no such chip and the HAL may not set that property, so it is left out.
constexpr SocName kSocNames[] = {
        {"qca6490", "hastings"}, {"qca6390", "hastings"}, {"kiwi", "hamilton"},
        {"wcn7850", "hamilton"}, {"peach", "ganges"},     {"wcn786x", "ganges"},
        {"wcn3990", "cherokee"}, {"wcn6750", "moselle"},  {"wcn6450", "evros"},
};

// Sets persist.vendor.qcom.bluetooth.soc from the chip's compatible string
// unless it is already set, before the HCI implementation reads it.
void SetSocName() {
    char value[PROPERTY_VALUE_MAX] = {};
    if (property_get(kSocProperty, value, "") > 0) {
        ALOGI("SoC name %s", value);
        return;
    }
    const int fd = open(kBtPowerDevice, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        ALOGE("Cannot open %s: %s", kBtPowerDevice, strerror(errno));
        return;
    }
    char chipset[kChipsetIdSize + 1] = {};
    const int result = ioctl(fd, kGetChipsetId, chipset);
    const int error = errno;
    close(fd);
    if (result < 0) {
        ALOGE("Cannot read the Bluetooth chipset: %s", strerror(error));
        return;
    }
    for (const SocName& soc : kSocNames) {
        if (strstr(chipset, soc.chipset) != nullptr) {
            if (property_set(kSocProperty, soc.name) != 0) {
                ALOGE("Cannot set %s", kSocProperty);
            } else {
                ALOGI("SoC name %s (chipset %s)", soc.name, chipset);
            }
            return;
        }
    }
    ALOGE("Unknown Bluetooth chipset %s", chipset);
}

}  // namespace

int main() {
    using android::OK;
    using android::status_t;
    using android::hardware::configureRpcThreadpool;
    using android::hardware::joinRpcThreadpool;
    using android::hardware::registerPassthroughServiceImplementation;

    InstallSeccompFilter();
    // As the stock service: files the HAL creates get mode 0644 at most.
    umask(022);
    configureRpcThreadpool(1, true /* callerWillJoin */);
    SetSocName();

    status_t status =
            registerPassthroughServiceImplementation<android::hardware::bluetooth::V1_1::IBluetoothHci>();
    if (status != OK) {
        ALOGE("Cannot register IBluetoothHci 1.1: %d", status);
        status = registerPassthroughServiceImplementation<
                android::hardware::bluetooth::V1_0::IBluetoothHci>();
        if (status != OK) {
            ALOGE("Cannot register IBluetoothHci 1.0: %d", status);
        }
    }
    // As the stock service, stay up even when registration failed, so init
    // does not restart the HAL in a loop.
    joinRpcThreadpool();
    return 1;
}
