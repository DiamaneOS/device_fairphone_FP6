// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The qcom_stats driver's ioctl interface (/dev/stats), the only sleep counter
// interface the FP6 kernel offers without debugfs. Source:
// drivers/soc/qcom/qcom_stats.c in the pinned kernel_qcom_msm-6.1, bound to the
// "qcom,rpmh-stats-v4" node of volcano.dtsi.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace fp6::powerstats {

// struct sleep_stats, as the driver copies it out.
struct SleepStats {
    uint32_t statType;  // record name, four characters ("aosd"), SoC records only
    uint32_t count;     // times the state was entered
    uint64_t lastEnteredAt;
    uint64_t lastExitedAt;
    uint64_t accumulated;  // includes the current stay if the state is active now
};
static_assert(sizeof(SleepStats) == 32, "layout of struct sleep_stats");

// The driver counts in 19.2 MHz QTimer ticks from SoC power-on, so the last
// entry times include the few seconds before the kernel starts.
constexpr uint64_t kTicksPerMs = 19200;

// ioctl numbers (_IOR(0x9d, nr, struct sleep_stats *)). Only these seven are
// used, and SELinux allows only these (sepolicy/fp6/power_stats.te). The
// driver maps the subsystem numbers to its subsystem table by position, and
// only modem, WPSS, ADSP and CDSP still match after newer entries were
// inserted: 4 (ADSP island) reads SLPI and 0 (APSS) reads the display entry.
// 13 (DDR frequency residency) sends a request to the AOSS for each read.
enum StatsIoctl : unsigned int {
    kModem = 1,
    kWpss = 2,
    kAdsp = 3,
    kCdsp = 5,
    kAosd = 10,
    kCxsd = 11,
    kDdr = 12,
};

unsigned int IoctlCommand(StatsIoctl nr);

// The record name in statType ("aosd", "cxsd", "ddr"), lower case, without
// padding.
std::string RecordName(uint32_t statType);

struct StateSource {
    const char* name;    // power stats state name
    StatsIoctl ioctlNr;  // where its counters come from
    const char* record;  // record name the driver must report, SoC records only
};

struct EntitySource {
    const char* name;  // power stats entity name
    std::vector<StateSource> states;
};

// The power entities the HAL reports, in id order (entity and state ids are
// their positions).
const std::vector<EntitySource>& Entities();

}  // namespace fp6::powerstats
