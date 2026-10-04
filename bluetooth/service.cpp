// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Bluetooth HCI service for the FP6. It registers the stock Qualcomm HCI
// implementation (android.hardware.bluetooth@1.1-impl-qti.so) through HIDL
// passthrough, as the stock android.hardware.bluetooth@1.1-service-qti does,
// with the same SoC-name step first. Unlike the stock service it does not link
// the FM, ANT, SAR, config-store and TPI libraries, which the stock service
// links but never registers on this device, and it does not report
// connectivity-proxy (Xpan) support to the BT FM codec driver, whose node the
// Bluetooth user cannot open here.

#define LOG_TAG "android.hardware.bluetooth@1.1-service.fp6"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <android/hardware/bluetooth/1.0/IBluetoothHci.h>
#include <android/hardware/bluetooth/1.1/IBluetoothHci.h>
#include <cutils/properties.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/LegacySupport.h>
#include <log/log.h>

namespace {

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
