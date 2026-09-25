// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <vector>

#include <aidl/android/hardware/biometrics/fingerprint/BnSession.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISessionCallback.h>

#include "LegacyHal.h"

namespace aidl::android::hardware::biometrics::fingerprint {

using common::ICancellationSignal;
using common::OperationContext;
using keymaster::HardwareAuthToken;

// Serialises every call into the FP6 module. The module is one global device;
// its callbacks (Session::onMessage) never take this lock, so a call that
// reports synchronously cannot deadlock. Lock order: this lock, then a
// session's own state lock.
std::mutex& moduleMutex();

// Translates one AIDL session into calls on the legacy FP6 module. The module
// and its trusted app keep templates, challenges, authenticator ids, lockout
// counters and HAT signing; this layer only relays requests and results.
//
// Each request is an operation with an id and a kind. An operation ends with
// exactly one terminal result: the module's own (error, success, lockout, ...)
// or, when the module fails a call without reporting anything, an
// UNABLE_TO_PROCESS fallback. Module messages that do not belong to the
// running operation, or arrive after it ended, are dropped. Cancellation only
// reaches the operation it was issued for.
//
// The service process has more than one binder thread (the module's own
// service thread joins the pool), so every entry point is safe to call
// concurrently.
//
// Authentication honours "Touch to unlock anytime": when SystemUI marks the
// request press-to-auth and the screen is off (NO_UI or AOD), matches are held
// back and the sensor is re-armed; a match from the last second is delivered
// once a context update reports the screen on (the power button woke it).
class Session : public BnSession {
  public:
    enum class OpKind {
        kNone,
        kEnroll,
        kAuthenticate,
        kDetectInteraction,
        kEnumerate,
        kRemove,
        kInvalidateAuthenticatorId,
        kResetLockout,
    };

    // onDetach is called once when the session is closed or its client dies.
    Session(fingerprint_device_t* device, int32_t userId, std::shared_ptr<ISessionCallback> cb,
            std::function<void(const Session*)> onDetach = nullptr);
    ~Session() override;

    // Links to the client's death; call once after construction.
    void watchClient();

    // Called for every message from the module while this is the active session.
    void onMessage(const fingerprint_msg_t* msg);

    // Cancels operation `op` if it is still this session's running operation.
    void cancel(uint64_t op);

    // Stops delivering messages, cancellations and callbacks. A dead client
    // can no longer cancel, so its running operation is cancelled here.
    void detach(bool clientDied);

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
    // Starts a new operation; the caller holds moduleMutex().
    uint64_t beginOperation(OpKind kind);
    // True while `op` is this session's running, unfinished operation; caller holds mMutex.
    bool runningLocked(uint64_t op) const;
    // True while an operation of `kind` is running; caller holds mMutex.
    bool runningLocked(OpKind kind) const;
    // Records the press-to-auth setting and display state; caller holds mMutex.
    void updateContextLocked(const OperationContext& context);
    // True while authentication results must wait for the screen to turn on.
    bool screenGatedLocked() const;
    // Starts authentication again after a held match; the module stops after one.
    void rearm(uint64_t op);
    // Stops the sensor after `op` ended outside the module (a held match was
    // delivered), unless another operation has started since.
    void stopSensorAfter(uint64_t op);
    // Reports UNABLE_TO_PROCESS for `op` unless it already has a terminal result.
    void failOperation(uint64_t op);
    // Marks the running operation finished; the caller holds mMutex.
    void endOperationLocked();
    std::shared_ptr<ICancellationSignal> cancellationSignal(uint64_t op);
    static void onClientDied(void* cookie);

    fingerprint_device_t* mDevice;
    const int32_t mUserId;
    const std::shared_ptr<ISessionCallback> mCb;
    std::function<void(const Session*)> mOnDetach;
    ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;

    // Guards the operation state and orders callbacks to the framework.
    std::mutex mMutex;
    uint64_t mNextOp = 0;
    uint64_t mOp = 0;         // running operation, 0 if none
    OpKind mOpKind = OpKind::kNone;
    bool mOpEnded = true;     // mOp already delivered its terminal result
    bool mDetached = false;
    std::vector<int32_t> mEnumerated;
    std::vector<int32_t> mRemoved;

    // Authentication in progress (mOp) and the context SystemUI gave for it.
    int64_t mAuthOperationId = 0;
    bool mPressToAuth = false;
    common::DisplayState mDisplayState = common::DisplayState::UNKNOWN;
    // A match that arrived while the screen was off.
    bool mHeldMatch = false;
    int32_t mHeldEnrollmentId = 0;
    HardwareAuthToken mHeldHat;
    std::chrono::steady_clock::time_point mHeldAt;
};

}  // namespace aidl::android::hardware::biometrics::fingerprint
