// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <mutex>
#include <vector>

#include <aidl/android/hardware/biometrics/fingerprint/BnSession.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISessionCallback.h>

#include "LegacyHal.h"

namespace aidl::android::hardware::biometrics::fingerprint {

using common::ICancellationSignal;
using common::OperationContext;
using keymaster::HardwareAuthToken;

// Translates one AIDL session into calls on the legacy FP6 module. The module
// and its trusted app keep templates, challenges, authenticator ids, lockout
// counters and HAT signing; this layer only relays requests and results.
class Session : public BnSession {
  public:
    Session(fingerprint_device_t* device, int32_t userId, std::shared_ptr<ISessionCallback> cb);

    // Called for every message from the module while this is the active session.
    void onMessage(const fingerprint_msg_t* msg);

    ndk::ScopedAStatus generateChallenge() override;
    ndk::ScopedAStatus revokeChallenge(int64_t challenge) override;
    ndk::ScopedAStatus enroll(const HardwareAuthToken& hat,
                              std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus authenticate(int64_t operationId,
                                    std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus detectInteraction(std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus enumerateEnrollments() override;
    ndk::ScopedAStatus removeEnrollments(const std::vector<int32_t>& enrollmentIds) override;
    ndk::ScopedAStatus getAuthenticatorId() override;
    ndk::ScopedAStatus invalidateAuthenticatorId() override;
    ndk::ScopedAStatus resetLockout(const HardwareAuthToken& hat) override;
    ndk::ScopedAStatus close() override;

    ndk::ScopedAStatus onPointerDown(int32_t pointerId, int32_t x, int32_t y, float minor,
                                     float major) override;
    ndk::ScopedAStatus onPointerUp(int32_t pointerId) override;
    ndk::ScopedAStatus onUiReady() override;
    ndk::ScopedAStatus authenticateWithContext(int64_t operationId, const OperationContext& context,
                                               std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus enrollWithContext(const HardwareAuthToken& hat,
                                         const OperationContext& context,
                                         std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus detectInteractionWithContext(
            const OperationContext& context, std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus onPointerDownWithContext(const PointerContext& context) override;
    ndk::ScopedAStatus onPointerUpWithContext(const PointerContext& context) override;
    ndk::ScopedAStatus onContextChanged(const OperationContext& context) override;
    ndk::ScopedAStatus onPointerCancelWithContext(const PointerContext& context) override;
    ndk::ScopedAStatus setIgnoreDisplayTouches(bool shouldIgnore) override;

  private:
    std::shared_ptr<ICancellationSignal> cancellationSignal();
    void reportError(Error error);

    fingerprint_device_t* mDevice;
    const int32_t mUserId;
    const std::shared_ptr<ISessionCallback> mCb;

    // Guards the collected ids; module messages arrive on its own thread.
    std::mutex mMutex;
    std::vector<int32_t> mEnumerated;
    std::vector<int32_t> mRemoved;
};

}  // namespace aidl::android::hardware::biometrics::fingerprint
