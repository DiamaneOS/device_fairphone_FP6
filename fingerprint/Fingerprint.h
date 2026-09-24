// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>

#include "LegacyHal.h"

namespace aidl::android::hardware::biometrics::fingerprint {

class Session;

class Fingerprint : public BnFingerprint {
  public:
    // Opens the FP6 module; returns nullptr if it is unavailable.
    static std::shared_ptr<Fingerprint> create();
    explicit Fingerprint(fingerprint_device_t* device);

    ndk::ScopedAStatus getSensorProps(std::vector<SensorProps>* out) override;
    ndk::ScopedAStatus createSession(int32_t sensorId, int32_t userId,
                                     const std::shared_ptr<ISessionCallback>& cb,
                                     std::shared_ptr<ISession>* out) override;

  private:
    static void notify(const fingerprint_msg_t* msg);

    fingerprint_device_t* mDevice;
    std::mutex mSessionMutex;
    std::shared_ptr<Session> mSession;
};

}  // namespace aidl::android::hardware::biometrics::fingerprint
