// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include "Record.h"

#include <cstdlib>
#include <vector>

#include <android-base/parseint.h>
#include <android-base/strings.h>

namespace timekeep {

namespace {

constexpr int kVersion = 1;

// The PMIC counter is 32 bits wide.
constexpr int64_t kMaxRtcSec = UINT32_MAX;

// Plausible wall-clock times, 2026-01-01 to 2100-01-01: a damaged record or
// counter must not move the clock decades ahead. No build is older than 2026,
// and system_server raises an earlier clock to the build date at every boot.
constexpr int64_t kMinWallMs = 1767225600000;
constexpr int64_t kMaxWallMs = 4102444800000;

bool valid(const Record& record) {
    return record.rtcSec >= 0 && record.rtcSec <= kMaxRtcSec &&
           record.offsetMs >= -kMaxRtcSec * 1000 && record.offsetMs < kMaxWallMs &&
           record.wallMs() >= kMinWallMs && record.wallMs() < kMaxWallMs;
}

}  // namespace

std::optional<Record> parseRecord(std::string_view text) {
    if (text.empty() || text.back() != '\n') return std::nullopt;
    text.remove_suffix(1);
    const std::vector<std::string> fields = android::base::Split(std::string(text), " ");
    int version;
    Record record;
    if (fields.size() != 3 || !android::base::ParseInt(fields[0], &version) ||
        version != kVersion || !android::base::ParseInt(fields[1], &record.offsetMs) ||
        !android::base::ParseInt(fields[2], &record.rtcSec) || !valid(record)) {
        return std::nullopt;
    }
    return record;
}

std::string formatRecord(const Record& record) {
    return std::to_string(kVersion) + " " + std::to_string(record.offsetMs) + " " +
           std::to_string(record.rtcSec) + "\n";
}

std::optional<int64_t> restoreTargetMs(const Record& saved, int64_t rtcSec, int64_t nowMs) {
    // Only a clock nobody has set since boot is changed: the kernel copied the
    // counter into it, and nothing else sets it before post-fs-data.
    if (std::abs(nowMs - rtcSec * 1000) >= kUnsetClockMarginMs) return std::nullopt;
    const int64_t savedWallMs = saved.wallMs();
    // Already at or past the last time known to have passed.
    if (nowMs >= savedWallMs) return std::nullopt;
    // The counter went back, so the PMIC lost power (battery out) and the
    // offset no longer applies. The last saved time is the best lower bound.
    if (rtcSec < saved.rtcSec) return savedWallMs;
    const int64_t targetMs = rtcSec * 1000 + saved.offsetMs;
    if (targetMs >= kMaxWallMs) return std::nullopt;
    return targetMs;
}

bool needsSave(const std::optional<Record>& saved, const Record& current) {
    if (!valid(current) || std::abs(current.offsetMs) < kUnsetClockMarginMs) return false;
    if (!saved || current.rtcSec < saved->rtcSec) return true;
    return std::abs(current.offsetMs - saved->offsetMs) >= kSaveThresholdMs ||
           current.wallMs() - saved->wallMs() >= kRefreshMs;
}

}  // namespace timekeep
