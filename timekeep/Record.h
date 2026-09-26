// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace timekeep {

// What timekeepd keeps across reboots: how far the system clock is ahead of the
// RTC counter, and the counter value this was measured at.
struct Record {
    int64_t offsetMs;  // CLOCK_REALTIME minus the RTC counter, in milliseconds
    int64_t rtcSec;    // RTC counter at the measurement

    // The wall-clock time of the measurement.
    int64_t wallMs() const { return rtcSec * 1000 + offsetMs; }
};

// Differences below this are RTC granularity (1 s), not a clock change.
constexpr int64_t kSaveThresholdMs = 1000;

// Save again after this long even if the offset did not change: after a power
// loss (battery out) the last saved time is all that is left, so it must stay
// recent.
constexpr int64_t kRefreshMs = 60 * 60 * 1000;

// A system clock this close to the RTC counter was never set: at boot the
// kernel copies the counter, which runs from the PMIC's last power loss, not
// from the Unix epoch.
constexpr int64_t kUnsetClockMarginMs = 60 * 1000;

// Parses the record file; nullopt if it is not exactly one valid record.
std::optional<Record> parseRecord(std::string_view text);

std::string formatRecord(const Record& record);

// The time to set at boot, or nullopt to leave the clock alone.
std::optional<int64_t> restoreTargetMs(const Record& saved, int64_t rtcSec, int64_t nowMs);

// Whether a new measurement should replace the saved record.
bool needsSave(const std::optional<Record>& saved, const Record& current);

}  // namespace timekeep
