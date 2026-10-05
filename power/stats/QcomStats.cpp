// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include "QcomStats.h"

#include <ctype.h>
#include <linux/ioctl.h>

namespace fp6::powerstats {

namespace {

constexpr unsigned int kStatsIoctlMagic = 0x9d;

}  // namespace

unsigned int IoctlCommand(StatsIoctl nr) {
    // The driver declares its commands with a pointer as the size argument,
    // so the encoded size is 8 on arm64; this HAL is built 64-bit only.
    return _IOR(kStatsIoctlMagic, nr, SleepStats*);
}

std::string RecordName(uint32_t statType) {
    std::string name;
    for (int i = 0; i < 4; ++i) {
        const unsigned char c = (statType >> (8 * i)) & 0xff;
        if (c == '\0' || c == ' ') break;
        name.push_back(static_cast<char>(tolower(c)));
    }
    return name;
}

const std::vector<EntitySource>& Entities() {
    // SoC: the AOSS sleep records (AOSD: whole-SoC deep sleep; CXSD: CX rail
    // collapse; DDR: DDR low-power mode). Subsystems: sleep time the remote
    // processors publish in shared memory (devicetree ss-name). WPSS runs the
    // Wi-Fi firmware.
    static const std::vector<EntitySource> kEntities = {
            {"SoC", {{"AOSD", kAosd, "aosd"}, {"CXSD", kCxsd, "cxsd"}, {"DDR", kDdr, "ddr"}}},
            {"Modem", {{"Sleep", kModem, nullptr}}},
            {"WPSS", {{"Sleep", kWpss, nullptr}}},
            {"ADSP", {{"Sleep", kAdsp, nullptr}}},
            {"CDSP", {{"Sleep", kCdsp, nullptr}}},
    };
    return kEntities;
}

}  // namespace fp6::powerstats
