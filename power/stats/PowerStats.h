// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <aidl/android/hardware/power/stats/BnPowerStats.h>

#include <vector>

namespace aidl::android::hardware::power::stats {

// IPowerStats for the FP6: state residency of the SoC sleep modes and the
// remote processors from /dev/stats (QcomStats.h). The FP6 has no on-device
// power monitor, so there are no energy meters or energy consumers.
class PowerStats : public BnPowerStats {
  public:
    PowerStats();

    ndk::ScopedAStatus getPowerEntityInfo(std::vector<PowerEntity>* _aidl_return) override;
    ndk::ScopedAStatus getStateResidency(const std::vector<int32_t>& in_powerEntityIds,
                                         std::vector<StateResidencyResult>* _aidl_return) override;
    ndk::ScopedAStatus getEnergyConsumerInfo(std::vector<EnergyConsumer>* _aidl_return) override;
    ndk::ScopedAStatus getEnergyConsumed(const std::vector<int32_t>& in_energyConsumerIds,
                                         std::vector<EnergyConsumerResult>* _aidl_return) override;
    ndk::ScopedAStatus getEnergyMeterInfo(std::vector<Channel>* _aidl_return) override;
    ndk::ScopedAStatus readEnergyMeter(const std::vector<int32_t>& in_channelIds,
                                       std::vector<EnergyMeasurement>* _aidl_return) override;

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;

  private:
    // Reads every state of one entity; false if any read fails, so an entity
    // is reported whole or not at all.
    bool readEntity(int fd, size_t index, std::vector<StateResidency>* residencies);

    std::vector<PowerEntity> mEntities;
};

}  // namespace aidl::android::hardware::power::stats
