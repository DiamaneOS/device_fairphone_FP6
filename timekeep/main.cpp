// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// timekeepd keeps the system clock across reboots. The FP6 PMIC RTC cannot be
// set from Linux (rtc-pm8xxx without allow-set-time): it counts seconds from
// its last power loss, and at boot the kernel copies that count into the
// clock. The daemon saves how far the clock is ahead of the counter whenever
// the clock changes, and at least hourly. At post-fs-data, `timekeepd
// --restore` sets the clock to the counter plus that offset and exits; it is
// the only part that may set the clock. No modem, network or app is involved.

#define LOG_TAG "timekeepd"

#include <fcntl.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

#include "Record.h"

using android::base::unique_fd;
using timekeep::Record;

namespace {

constexpr char kRtcPath[] = "/sys/class/rtc/rtc0/since_epoch";
constexpr char kDirPath[] = "/data/vendor/timekeepd";
constexpr char kRecordPath[] = "/data/vendor/timekeepd/offset";
constexpr char kTempPath[] = "/data/vendor/timekeepd/offset.tmp";

// Also measure this often while the phone stays awake, so the saved time is
// refreshed and RTC drift followed. Clock changes and every resume from
// suspend wake the daemon in any case.
constexpr time_t kRecheckSec = timekeep::kRefreshMs / 1000;

// Reads a small file with open and read only; the policy grants nothing else.
std::optional<std::string> readSmallFile(const char* path) {
    unique_fd fd(TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_CLOEXEC)));
    if (fd < 0) return std::nullopt;
    char buf[64];
    const ssize_t n = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
    if (n < 0) return std::nullopt;
    if (n == sizeof(buf)) {
        errno = EFBIG;
        return std::nullopt;
    }
    return std::string(buf, n);
}

std::optional<int64_t> readRtcSec() {
    // Log a failing RTC once, not on every resume.
    static bool failed = false;
    const auto text = readSmallFile(kRtcPath);
    int64_t sec;
    if (!text) {
        if (!failed) PLOG(ERROR) << "Cannot read " << kRtcPath;
    } else if (!android::base::ParseInt(android::base::Trim(*text), &sec, int64_t{0},
                                        int64_t{UINT32_MAX})) {
        if (!failed) LOG(ERROR) << "Unexpected value in " << kRtcPath;
    } else {
        failed = false;
        return sec;
    }
    failed = true;
    return std::nullopt;
}

int64_t realtimeMs() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return int64_t{ts.tv_sec} * 1000 + ts.tv_nsec / 1000000;
}

std::optional<Record> loadRecord() {
    const auto text = readSmallFile(kRecordPath);
    if (!text) {
        if (errno == ENOENT) {
            LOG(INFO) << "No saved offset yet";
        } else {
            PLOG(ERROR) << "Cannot read " << kRecordPath;
        }
        return std::nullopt;
    }
    auto record = timekeep::parseRecord(*text);
    if (!record) LOG(ERROR) << "Ignoring invalid " << kRecordPath;
    return record;
}

// Makes a completed rename survive a power loss; on f2fs it would otherwise
// wait for the next checkpoint.
bool syncDir() {
    unique_fd fd(TEMP_FAILURE_RETRY(open(kDirPath, O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
    return fd >= 0 && fsync(fd) == 0;
}

// Replaces the record atomically, so a crash or power loss leaves either the
// old or the new one.
bool saveRecord(const Record& record) {
    // measure() retries on every wake: log a failing save once, not each time.
    static bool failed = false;
    unique_fd fd(TEMP_FAILURE_RETRY(
            open(kTempPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600)));
    if (fd < 0 || !android::base::WriteStringToFd(timekeep::formatRecord(record), fd) ||
        fsync(fd) != 0) {
        if (!failed) PLOG(ERROR) << "Cannot write " << kTempPath;
    } else if (rename(kTempPath, kRecordPath) != 0) {
        if (!failed) PLOG(ERROR) << "Cannot replace " << kRecordPath;
    } else if (!syncDir()) {
        if (!failed) PLOG(ERROR) << "Cannot sync " << kDirPath;
    } else {
        failed = false;
        return true;
    }
    failed = true;
    return false;
}

// Makes the next read of the timer fd return when CLOCK_REALTIME is set (by
// Android; the kernel reports each resume from suspend the same way) or after
// kRecheckSec.
void arm(int tfd) {
    itimerspec spec = {};
    spec.it_value.tv_sec = realtimeMs() / 1000 + kRecheckSec;
    // ECANCELED: the clock changed or the phone resumed since the last read.
    // The timer is armed anyway, and the caller measures next.
    if (timerfd_settime(tfd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &spec, nullptr) != 0 &&
        errno != ECANCELED) {
        PLOG(FATAL) << "timerfd_settime";
    }
}

// Sets the clock from the saved offset, once at boot (vendor.timekeepd-restore).
void restore() {
    const auto saved = loadRecord();
    if (!saved) return;
    const auto rtcSec = readRtcSec();
    if (!rtcSec) return;
    const int64_t nowMs = realtimeMs();
    const auto targetMs = timekeep::restoreTargetMs(*saved, *rtcSec, nowMs);
    if (!targetMs) {
        LOG(INFO) << "Clock already set or past the last saved time; not changed";
        return;
    }
    if (*rtcSec < saved->rtcSec) {
        LOG(WARNING) << "RTC counter went back (power loss); restoring the last saved time";
    }
    const timespec ts = {
            .tv_sec = static_cast<time_t>(*targetMs / 1000),
            .tv_nsec = static_cast<long>(*targetMs % 1000) * 1000000,
    };
    if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
        PLOG(ERROR) << "Cannot set the clock";
        return;
    }
    LOG(INFO) << "Clock restored to " << *targetMs / 1000 << " (Unix time)";
}

// Measures the offset and saves it if it changed or is due for a refresh.
void measure(std::optional<Record>& saved) {
    const auto rtcSec = readRtcSec();
    if (!rtcSec) return;
    const Record current = {realtimeMs() - *rtcSec * 1000, *rtcSec};
    if (!timekeep::needsSave(saved, current) || !saveRecord(current)) return;
    LOG(INFO) << "Saved offset " << current.offsetMs << " ms at RTC " << current.rtcSec;
    saved = current;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && strcmp(argv[1], "--restore") == 0) {
        restore();
        return 0;
    }
    if (argc != 1) LOG(FATAL) << "Usage: timekeepd [--restore]";
    unique_fd tfd(timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC));
    if (tfd < 0) PLOG(FATAL) << "timerfd_create";
    // Arm before the first measurement, so that no change is missed.
    arm(tfd);
    std::optional<Record> saved = loadRecord();
    for (;;) {
        measure(saved);
        uint64_t expirations;
        if (read(tfd, &expirations, sizeof(expirations)) < 0 && errno != ECANCELED &&
            errno != EINTR) {
            PLOG(FATAL) << "Cannot read the timer";
        }
        arm(tfd);
    }
}
