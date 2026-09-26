// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include <gtest/gtest.h>

#include "Record.h"

namespace timekeep {
namespace {

// 2026-09-26T12:00:00Z and an RTC counter three days after a power loss.
constexpr int64_t kWallMs = 1790424000000;
constexpr int64_t kRtcSec = 260000;
constexpr Record kSaved = {kWallMs - kRtcSec * 1000, kRtcSec};

// 2100-01-01T00:00:00Z, the first time timekeepd refuses.
constexpr int64_t k2100Ms = 4102444800000;

TEST(RecordTest, FormatParseRoundTrip) {
    const auto parsed = parseRecord(formatRecord(kSaved));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->offsetMs, kSaved.offsetMs);
    EXPECT_EQ(parsed->rtcSec, kSaved.rtcSec);
    EXPECT_EQ(parsed->wallMs(), kWallMs);
}

TEST(RecordTest, RejectsMalformedRecords) {
    EXPECT_FALSE(parseRecord(""));
    EXPECT_FALSE(parseRecord("1 1790164000000 260000"));        // no newline
    EXPECT_FALSE(parseRecord("2 1790164000000 260000\n"));      // unknown version
    EXPECT_FALSE(parseRecord("1 1790164000000\n"));             // missing field
    EXPECT_FALSE(parseRecord("1  1790164000000 260000\n"));     // empty field
    EXPECT_FALSE(parseRecord("1 1790164000000 260000 7\n"));    // extra field
    EXPECT_FALSE(parseRecord("1 1790164000000 26000x\n"));      // not a number
    EXPECT_FALSE(parseRecord("1 1790164000000 -1\n"));          // negative counter
    EXPECT_FALSE(parseRecord("1 1790164000000 4294967296\n"));  // counter over 32 bits
    EXPECT_FALSE(parseRecord("1 -260000000 260000\n"));         // wall time 0
    EXPECT_FALSE(parseRecord("1 99999999999999999 0\n"));       // after year 9999
    EXPECT_FALSE(parseRecord("1 1790164000000 99999999999999999999\n"));
}

TEST(RecordTest, RejectsImplausibleTimes) {
    // 2100-01-01 and later: a damaged record must not move the clock decades on.
    EXPECT_FALSE(parseRecord(formatRecord(Record{k2100Ms - kRtcSec * 1000, kRtcSec})));
    EXPECT_TRUE(parseRecord(formatRecord(Record{k2100Ms - 1 - kRtcSec * 1000, kRtcSec})));
    // Before 2026-01-01.
    EXPECT_FALSE(parseRecord(formatRecord(Record{1767225599999 - kRtcSec * 1000, kRtcSec})));
    EXPECT_TRUE(parseRecord(formatRecord(Record{1767225600000 - kRtcSec * 1000, kRtcSec})));
    // The restore target is bounded too: here the counter ran on past 2100.
    const Record late = {k2100Ms - 5000, 0};
    EXPECT_TRUE(parseRecord(formatRecord(late)));
    EXPECT_EQ(restoreTargetMs(late, 4, 4 * 1000), k2100Ms - 1000);
    EXPECT_FALSE(restoreTargetMs(late, 5, 5 * 1000));
}

TEST(RecordTest, RestoresCounterPlusOffset) {
    // Off for an hour: the counter ran on, so the clock is set an hour later.
    const auto target = restoreTargetMs(kSaved, kRtcSec + 3600, (kRtcSec + 3600) * 1000);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(*target, kWallMs + 3600 * 1000);
}

TEST(RecordTest, RestoresLastSavedTimeAfterCounterReset) {
    // Battery out: the counter restarted near zero. Never go before the last
    // saved time.
    const auto target = restoreTargetMs(kSaved, 42, 42 * 1000);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(*target, kWallMs);
}

TEST(RecordTest, LeavesClockThatIsAlreadySet) {
    // Daemon restart, or the clock was set before the daemon ran.
    EXPECT_FALSE(restoreTargetMs(kSaved, kRtcSec + 10, kWallMs + 10 * 1000));
    EXPECT_FALSE(restoreTargetMs(kSaved, kRtcSec, kWallMs));
    EXPECT_FALSE(restoreTargetMs(kSaved, 42, kWallMs + 1));
    // Set back a day after boot and not saved yet: Android's change stands.
    EXPECT_FALSE(restoreTargetMs(kSaved, kRtcSec + 100, kWallMs - 86400 * 1000));
}

TEST(RecordTest, LeavesClockAtCounterLimit) {
    // The counter at its 32-bit limit is past the saved time: nothing to do,
    // and no overflow on the way.
    EXPECT_FALSE(restoreTargetMs(kSaved, UINT32_MAX, int64_t{UINT32_MAX} * 1000));
    EXPECT_FALSE(restoreTargetMs(kSaved, UINT32_MAX, 42 * 1000));
}

TEST(RecordTest, SavesOnlyRealChanges) {
    const Record same = {kSaved.offsetMs + 999, kRtcSec + 100};
    const Record later = {kSaved.offsetMs + 1000, kRtcSec + 100};
    const Record earlier = {kSaved.offsetMs - 5000, kRtcSec + 100};
    const Record afterReset = {kSaved.offsetMs + kRtcSec * 1000, 0};
    EXPECT_FALSE(needsSave(kSaved, kSaved));
    EXPECT_FALSE(needsSave(kSaved, same));  // RTC granularity
    EXPECT_TRUE(needsSave(kSaved, later));
    EXPECT_TRUE(needsSave(kSaved, earlier));
    EXPECT_TRUE(needsSave(kSaved, afterReset));
    EXPECT_TRUE(needsSave(std::nullopt, kSaved));
}

TEST(RecordTest, RefreshesTheSavedTimeHourly) {
    // Same offset: saved again once an hour has passed, so a battery pull
    // restores a recent time.
    EXPECT_FALSE(needsSave(kSaved, Record{kSaved.offsetMs, kRtcSec + 3599}));
    EXPECT_TRUE(needsSave(kSaved, Record{kSaved.offsetMs, kRtcSec + 3600}));
}

TEST(RecordTest, NeverSavesAnUnsetClock) {
    // At boot the kernel copies the counter into the clock; before Android
    // sets the time the offset is about zero and means nothing.
    EXPECT_FALSE(needsSave(std::nullopt, Record{350, kRtcSec}));
    EXPECT_FALSE(needsSave(kSaved, Record{-59 * 1000, kRtcSec}));
    // A record that could not be read back is not written either.
    EXPECT_FALSE(needsSave(std::nullopt, Record{kWallMs, -1}));
    EXPECT_FALSE(needsSave(std::nullopt, Record{k2100Ms - kRtcSec * 1000, kRtcSec}));
}

}  // namespace
}  // namespace timekeep
