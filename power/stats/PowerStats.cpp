// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include "PowerStats.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <numeric>
#include <string>
#include <utility>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>

#include "QcomStats.h"

namespace aidl::android::hardware::power::stats {

using ::android::base::StringPrintf;
using ::android::base::unique_fd;
using ::fp6::powerstats::Entities;
using ::fp6::powerstats::IoctlCommand;
using ::fp6::powerstats::kTicksPerMs;
using ::fp6::powerstats::RecordName;
using ::fp6::powerstats::SleepStats;

namespace {

constexpr char kStatsDevice[] = "/dev/stats";

StateResidency ToResidency(int32_t id, const SleepStats& stats) {
    return {
            .id = id,
            .totalTimeInStateMs = static_cast<int64_t>(stats.accumulated / kTicksPerMs),
            .totalStateEntryCount = stats.count,
            .lastEntryTimestampMs = static_cast<int64_t>(stats.lastEnteredAt / kTicksPerMs),
    };
}

}  // namespace

PowerStats::PowerStats() {
    const auto& entities = Entities();
    for (size_t i = 0; i < entities.size(); ++i) {
        PowerEntity entity = {.id = static_cast<int32_t>(i), .name = entities[i].name};
        for (size_t j = 0; j < entities[i].states.size(); ++j) {
            entity.states.push_back(
                    {.id = static_cast<int32_t>(j), .name = entities[i].states[j].name});
        }
        mEntities.push_back(std::move(entity));
    }
}

ndk::ScopedAStatus PowerStats::getPowerEntityInfo(std::vector<PowerEntity>* _aidl_return) {
    *_aidl_return = mEntities;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getStateResidency(const std::vector<int32_t>& in_powerEntityIds,
                                                 std::vector<StateResidencyResult>* _aidl_return) {
    std::vector<int32_t> ids = in_powerEntityIds;
    if (ids.empty()) {
        ids.resize(mEntities.size());
        std::iota(ids.begin(), ids.end(), 0);
    }
    for (const int32_t id : ids) {
        if (id < 0 || static_cast<size_t>(id) >= mEntities.size()) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }

    // Opened per request: the driver is a vendor_dlkm module, and the reads
    // are a few ioctls.
    unique_fd fd(TEMP_FAILURE_RETRY(open(kStatsDevice, O_RDONLY | O_CLOEXEC)));
    if (fd < 0) {
        PLOG(ERROR) << "Cannot open " << kStatsDevice;
        return ndk::ScopedAStatus::ok();
    }
    for (const int32_t id : ids) {
        StateResidencyResult result = {.id = id};
        if (readEntity(fd.get(), static_cast<size_t>(id), &result.stateResidencyData)) {
            _aidl_return->push_back(std::move(result));
        }
    }
    return ndk::ScopedAStatus::ok();
}

bool PowerStats::readEntity(int fd, size_t index, std::vector<StateResidency>* residencies) {
    const auto& entity = Entities()[index];
    for (size_t j = 0; j < entity.states.size(); ++j) {
        const auto& state = entity.states[j];
        SleepStats stats = {};
        // The driver returns -EIO for a subsystem whose counters are not in
        // shared memory yet (not booted since power-on).
        if (ioctl(fd, IoctlCommand(state.ioctlNr), &stats) != 0) {
            PLOG(WARNING) << "No counters for " << entity.name << " " << state.name;
            return false;
        }
        if (state.record != nullptr && RecordName(stats.statType) != state.record) {
            LOG(ERROR) << entity.name << " " << state.name << ": driver reports record '"
                       << RecordName(stats.statType) << "'";
            return false;
        }
        residencies->push_back(ToResidency(static_cast<int32_t>(j), stats));
    }
    return true;
}

ndk::ScopedAStatus PowerStats::getEnergyConsumerInfo(std::vector<EnergyConsumer>* _aidl_return) {
    _aidl_return->clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getEnergyConsumed(const std::vector<int32_t>& in_energyConsumerIds,
                                                 std::vector<EnergyConsumerResult>* _aidl_return) {
    _aidl_return->clear();
    if (!in_energyConsumerIds.empty()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getEnergyMeterInfo(std::vector<Channel>* _aidl_return) {
    _aidl_return->clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::readEnergyMeter(const std::vector<int32_t>& in_channelIds,
                                               std::vector<EnergyMeasurement>* _aidl_return) {
    _aidl_return->clear();
    if (!in_channelIds.empty()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    return ndk::ScopedAStatus::ok();
}

binder_status_t PowerStats::dump(int fd, const char** /*args*/, uint32_t /*numArgs*/) {
    std::vector<StateResidencyResult> results;
    getStateResidency({}, &results);

    std::string text =
            "State residency from /dev/stats (qcom_stats); no energy meters or consumers.\n";
    text += StringPrintf("%-8s %-6s %14s %12s %16s\n", "Entity", "State", "Time (ms)", "Entries",
                         "Last entry (ms)");
    for (const auto& entity : mEntities) {
        const StateResidencyResult* result = nullptr;
        for (const auto& r : results) {
            if (r.id == entity.id) result = &r;
        }
        if (result == nullptr) {
            text += StringPrintf("%-8s %-6s %14s\n", entity.name.c_str(), "-", "no data");
            continue;
        }
        for (const auto& residency : result->stateResidencyData) {
            text += StringPrintf("%-8s %-6s %14lld %12lld %16lld\n", entity.name.c_str(),
                                 entity.states[residency.id].name.c_str(),
                                 static_cast<long long>(residency.totalTimeInStateMs),
                                 static_cast<long long>(residency.totalStateEntryCount),
                                 static_cast<long long>(residency.lastEntryTimestampMs));
        }
    }
    if (!::android::base::WriteStringToFd(text, fd)) {
        return STATUS_FAILED_TRANSACTION;
    }
    return STATUS_OK;
}

}  // namespace aidl::android::hardware::power::stats
