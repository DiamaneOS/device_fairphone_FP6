// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <aidl/android/hardware/biometrics/fingerprint/BnSessionCallback.h>
#include <gtest/gtest.h>

#include "Session.h"

namespace aidl::android::hardware::biometrics::fingerprint {
namespace {

// Records callbacks as short strings so tests can assert exact sequences.
class RecordingCallback : public BnSessionCallback {
  public:
    std::vector<std::string> events;

    ndk::ScopedAStatus onChallengeGenerated(int64_t) override { return add("challenge"); }
    ndk::ScopedAStatus onChallengeRevoked(int64_t) override { return add("revoked"); }
    ndk::ScopedAStatus onAcquired(AcquiredInfo info, int32_t) override {
        return add("acquired " + std::to_string(static_cast<int>(info)));
    }
    ndk::ScopedAStatus onError(Error error, int32_t vendorCode) override {
        return add("error " + std::to_string(static_cast<int>(error)) + "/" +
                   std::to_string(vendorCode));
    }
    ndk::ScopedAStatus onEnrollmentProgress(int32_t id, int32_t remaining) override {
        return add("progress " + std::to_string(id) + "/" + std::to_string(remaining));
    }
    ndk::ScopedAStatus onAuthenticationSucceeded(int32_t id, const HardwareAuthToken&) override {
        return add("success " + std::to_string(id));
    }
    ndk::ScopedAStatus onAuthenticationFailed() override { return add("failed"); }
    ndk::ScopedAStatus onLockoutTimed(int64_t) override { return add("lockout timed"); }
    ndk::ScopedAStatus onLockoutPermanent() override { return add("lockout permanent"); }
    ndk::ScopedAStatus onLockoutCleared() override { return add("lockout cleared"); }
    ndk::ScopedAStatus onInteractionDetected() override { return add("interaction"); }
    ndk::ScopedAStatus onEnrollmentsEnumerated(const std::vector<int32_t>& ids) override {
        return add("enumerated " + std::to_string(ids.size()));
    }
    ndk::ScopedAStatus onEnrollmentsRemoved(const std::vector<int32_t>& ids) override {
        return add("removed " + std::to_string(ids.size()));
    }
    ndk::ScopedAStatus onAuthenticatorIdRetrieved(int64_t) override { return add("id"); }
    ndk::ScopedAStatus onAuthenticatorIdInvalidated(int64_t) override { return add("id invalidated"); }
    ndk::ScopedAStatus onSessionClosed() override { return add("closed"); }
    ndk::ScopedAStatus onAuthenticationSucceededWithResult(const AuthenticateSuccess&) override {
        return add("success with result");
    }

  private:
    ndk::ScopedAStatus add(std::string event) {
        events.push_back(std::move(event));
        return ndk::ScopedAStatus::ok();
    }
};

// Fake module. Each entry point does what the test configured, optionally
// reporting through the callback before it returns, as the FP6 module does.
struct FakeModule {
    fingerprint_device_t device = {};
    std::shared_ptr<Session> session;
    int result = 0;                 // value returned by the next call
    int syncError = 0;              // FINGERPRINT_ERROR sent before returning, if non-zero
    std::vector<uint32_t> removeFids;  // FINGERPRINT_TEMPLATE_REMOVED sent by remove()
    bool cancelReportsCanceled = false;  // cancel() sends CANCELED before returning, as the module does
    int cancels = 0;
    std::atomic<int> authenticates = 0;
    std::mutex callsMutex;
    std::vector<std::string> calls;  // module entry points in call order
};
FakeModule* gModule;

void sendError(int error) {
    fingerprint_msg_t msg = {};
    msg.type = FINGERPRINT_ERROR;
    msg.data.error = static_cast<fingerprint_error_t>(error);
    gModule->session->onMessage(&msg);
}

int finish() {
    if (gModule->syncError != 0) sendError(gModule->syncError);
    return gModule->result;
}

int fakeEnroll(fingerprint_device_t*, const hw_auth_token_t*, uint32_t, uint32_t) { return finish(); }
void record(const char* call) {
    std::lock_guard lock(gModule->callsMutex);
    gModule->calls.push_back(call);
}

int fakeAuthenticate(fingerprint_device_t*, uint64_t, uint32_t) {
    record("authenticate");
    gModule->authenticates++;
    return finish();
}
int fakeDetect(fingerprint_device_t*) { return finish(); }

// Writes the same layout as SystemUI's PressToAuthParcelable.
struct TestPressToAuth {
    static constexpr const char* descriptor =
            "com.google.hardware.biometrics.parcelables.fingerprint.PressToAuthParcelable";
    static const ndk::parcelable_stability_t _aidl_stability = ndk::STABILITY_VINTF;
    bool pressToAuthEnabled = false;
    binder_status_t writeToParcel(AParcel* parcel) const {
        int32_t start = AParcel_getDataPosition(parcel);
        AParcel_writeInt32(parcel, 0);
        AParcel_writeBool(parcel, pressToAuthEnabled);
        int32_t end = AParcel_getDataPosition(parcel);
        AParcel_setDataPosition(parcel, start);
        AParcel_writeInt32(parcel, end - start);
        return AParcel_setDataPosition(parcel, end);
    }
    binder_status_t readFromParcel(const AParcel*) { return STATUS_OK; }
};

OperationContext context(common::DisplayState display, bool pressToAuth) {
    OperationContext ctx;
    ctx.displayState = display;
    common::AuthenticateReason::Vendor vendor;
    vendor.extension.setParcelable(TestPressToAuth{.pressToAuthEnabled = pressToAuth});
    ctx.authenticateReason = common::AuthenticateReason::make<
            common::AuthenticateReason::Tag::vendorAuthenticateReason>(std::move(vendor));
    return ctx;
}

bool waitFor(const std::atomic<int>& value, int expected) {
    for (int i = 0; i < 100 && value < expected; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return value >= expected;
}
int fakeEnumerate(fingerprint_device_t*) { return finish(); }
int fakeResetLockout(fingerprint_device_t*, const hw_auth_token_t*) { return finish(); }
int fakeCancel(fingerprint_device_t*) {
    record("cancel");
    gModule->cancels++;
    if (gModule->cancelReportsCanceled) sendError(FINGERPRINT_ERROR_CANCELED);
    return 0;
}
int fakeRemove(fingerprint_device_t*, uint32_t gid, uint32_t fid) {
    fingerprint_msg_t msg = {};
    msg.type = FINGERPRINT_TEMPLATE_REMOVED;
    msg.data.removed.finger = {.gid = gid, .fid = fid};
    gModule->session->onMessage(&msg);
    return finish();
}

class SessionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        gModule = &mModule;
        auto& dev = mModule.device;
        dev.enroll = fakeEnroll;
        dev.authenticate = fakeAuthenticate;
        dev.enumerate = fakeEnumerate;
        dev.cancel = fakeCancel;
        dev.remove = fakeRemove;
        dev.reserved[fp6::kResetLockout] = reinterpret_cast<void*>(fakeResetLockout);
        dev.reserved[fp6::kDetectInteraction] = reinterpret_cast<void*>(fakeDetect);
        mCb = ndk::SharedRefBase::make<RecordingCallback>();
        mSession = ndk::SharedRefBase::make<Session>(&dev, kUser, mCb);
        mModule.session = mSession;
    }
    void TearDown() override { gModule = nullptr; }

    void deliver(fingerprint_msg_t msg) { mSession->onMessage(&msg); }
    fingerprint_msg_t authenticated(uint32_t gid, uint32_t fid) {
        fingerprint_msg_t msg = {};
        msg.type = FINGERPRINT_AUTHENTICATED;
        msg.data.authenticated.finger = {.gid = gid, .fid = fid};
        return msg;
    }
    fingerprint_msg_t lockout(uint32_t kind) {
        fingerprint_msg_t msg = {};
        msg.type = static_cast<fingerprint_msg_type_t>(fp6::kMsgLockout);
        fp6::LockoutMsg payload = {.kind = kind, .reserved = 0, .durationMillis = 30000};
        std::memcpy(&msg.data, &payload, sizeof(payload));
        return msg;
    }

    static constexpr int32_t kUser = 10;
    FakeModule mModule;
    std::shared_ptr<RecordingCallback> mCb;
    std::shared_ptr<Session> mSession;
    std::shared_ptr<common::ICancellationSignal> mSignal;
};

using Events = std::vector<std::string>;

TEST_F(SessionTest, RejectedLockoutResetReportsOneError) {
    // The module reports UNABLE_TO_PROCESS itself and also returns -208.
    mModule.syncError = FINGERPRINT_ERROR_UNABLE_TO_PROCESS;
    mModule.result = -208;
    mSession->resetLockout(HardwareAuthToken{});
    EXPECT_EQ(mCb->events, (Events{"error 2/0"}));
}

TEST_F(SessionTest, RejectedEnrollmentKeepsModuleError) {
    mModule.syncError = FINGERPRINT_ERROR_NO_SPACE;
    mModule.result = -1;
    mSession->enroll(HardwareAuthToken{}, &mSignal);
    EXPECT_EQ(mCb->events, (Events{"error 4/0"}));
}

TEST_F(SessionTest, SilentFailureReportsFallbackError) {
    mModule.result = -5;
    mSession->authenticate(1, &mSignal);
    EXPECT_EQ(mCb->events, (Events{"error 2/0"}));
}

TEST_F(SessionTest, EndedOperationDoesNotLeakIntoNext) {
    mModule.result = -5;
    mSession->authenticate(1, &mSignal);
    mModule.result = 0;
    mSession->authenticate(2, &mSignal);
    deliver(authenticated(kUser, 3));
    EXPECT_EQ(mCb->events, (Events{"error 2/0", "success 3"}));
}

TEST_F(SessionTest, AsynchronousSuccessHasNoError) {
    mSession->authenticate(1, &mSignal);
    deliver(authenticated(kUser, 0));
    deliver(authenticated(kUser, 3));
    EXPECT_EQ(mCb->events, (Events{"failed", "success 3"}));
}

TEST_F(SessionTest, MatchForAnotherUserIsRejected) {
    mSession->authenticate(1, &mSignal);
    deliver(authenticated(kUser + 1, 3));
    EXPECT_EQ(mCb->events, (Events{"failed"}));
}

TEST_F(SessionTest, ScreenOffMatchWaitsForScreenOn) {
    // "Touch to unlock anytime" off: a touch with the screen off does not unlock.
    mSession->authenticateWithContext(1, context(common::DisplayState::NO_UI, true), &mSignal);
    deliver(authenticated(kUser, 3));
    EXPECT_TRUE(mCb->events.empty());
    EXPECT_TRUE(waitFor(mModule.authenticates, 2));  // the sensor listens again
    // The power press turns the screen on; the match from just before counts.
    mSession->onContextChanged(context(common::DisplayState::LOCKSCREEN, true));
    EXPECT_EQ(mCb->events, (Events{"success 3"}));
}

TEST_F(SessionTest, ScreenOffFailuresAreSilent) {
    mSession->authenticateWithContext(1, context(common::DisplayState::AOD, true), &mSignal);
    deliver(authenticated(kUser, 0));
    EXPECT_TRUE(mCb->events.empty());
}

TEST_F(SessionTest, ScreenOnTouchUnlocksWithPressToAuth) {
    mSession->authenticateWithContext(1, context(common::DisplayState::LOCKSCREEN, true), &mSignal);
    deliver(authenticated(kUser, 3));
    EXPECT_EQ(mCb->events, (Events{"success 3"}));
}

TEST_F(SessionTest, TouchToUnlockAnytimeUnlocksWithScreenOff) {
    mSession->authenticateWithContext(1, context(common::DisplayState::NO_UI, false), &mSignal);
    deliver(authenticated(kUser, 3));
    EXPECT_EQ(mCb->events, (Events{"success 3"}));
}

TEST_F(SessionTest, StaleCancellationIsIgnored) {
    std::shared_ptr<common::ICancellationSignal> first, second;
    mSession->authenticate(1, &first);
    mSession->authenticate(2, &second);
    first->cancel();
    EXPECT_EQ(mModule.cancels, 0);
    second->cancel();
    EXPECT_EQ(mModule.cancels, 1);
}

TEST_F(SessionTest, CancellationAfterCompletionIsIgnored) {
    mSession->authenticate(1, &mSignal);
    deliver(authenticated(kUser, 3));
    mSignal->cancel();
    EXPECT_EQ(mModule.cancels, 0);
}

TEST_F(SessionTest, RemovalReportsRemovedIdsOnce) {
    mSession->removeEnrollments({3, 4});
    EXPECT_EQ(mCb->events, (Events{"removed 2"}));
}

TEST_F(SessionTest, ClosedSessionIgnoresLateMessages) {
    mSession->authenticate(1, &mSignal);
    deliver(authenticated(kUser, 3));
    mSession->close();
    sendError(FINGERPRINT_ERROR_CANCELED);
    mSignal->cancel();
    EXPECT_EQ(mCb->events, (Events{"success 3", "closed"}));
    EXPECT_EQ(mModule.cancels, 0);
}

TEST_F(SessionTest, DeadClientCancelsRunningOperation) {
    mSession->authenticate(1, &mSignal);
    mSession->detach(true);
    sendError(FINGERPRINT_ERROR_CANCELED);
    EXPECT_EQ(mModule.cancels, 1);
    EXPECT_TRUE(mCb->events.empty());
}

TEST_F(SessionTest, MatchAfterCancelIsDropped) {
    // A late scan result must not unlock after the framework cancelled.
    mModule.cancelReportsCanceled = true;
    mSession->authenticate(1, &mSignal);
    mSignal->cancel();
    deliver(authenticated(kUser, 3));
    EXPECT_EQ(mCb->events, (Events{"error 5/0"}));
}

TEST_F(SessionTest, ErrorWithoutOperationIsDropped) {
    sendError(FINGERPRINT_ERROR_HW_UNAVAILABLE);
    EXPECT_TRUE(mCb->events.empty());
}

TEST_F(SessionTest, LockoutClearedDoesNotEndAuthentication) {
    mSession->authenticate(1, &mSignal);
    deliver(lockout(fp6::kLockoutCleared));
    deliver(authenticated(kUser, 3));
    EXPECT_EQ(mCb->events, (Events{"lockout cleared", "success 3"}));
}

TEST_F(SessionTest, TimedLockoutEndsAuthenticationOnce) {
    mSession->authenticate(1, &mSignal);
    deliver(lockout(fp6::kLockoutTimed));
    sendError(FINGERPRINT_ERROR_LOCKOUT);  // a legacy error after the lockout message
    mSignal->cancel();
    EXPECT_EQ(mCb->events, (Events{"lockout timed"}));
    EXPECT_EQ(mModule.cancels, 0);
}

TEST_F(SessionTest, LockoutClearedEndsLockoutReset) {
    mSession->resetLockout(HardwareAuthToken{});
    deliver(lockout(fp6::kLockoutCleared));
    sendError(FINGERPRINT_ERROR_UNABLE_TO_PROCESS);
    EXPECT_EQ(mCb->events, (Events{"lockout cleared"}));
}

TEST_F(SessionTest, MatchDuringDetectionReportsInteractionOnly) {
    mSession->detectInteraction(&mSignal);
    deliver(authenticated(kUser, 3));
    deliver(authenticated(kUser, 3));
    EXPECT_EQ(mCb->events, (Events{"interaction"}));
}

TEST_F(SessionTest, PromptAfterKeyguardIsNotScreenGated) {
    // A request without the press-to-auth parcelable (BiometricPrompt) must not
    // inherit the keyguard's setting.
    mModule.cancelReportsCanceled = true;
    mSession->authenticateWithContext(1, context(common::DisplayState::NO_UI, true), &mSignal);
    mSignal->cancel();
    OperationContext prompt;
    prompt.displayState = common::DisplayState::NO_UI;
    prompt.authenticateReason = common::AuthenticateReason::make<
            common::AuthenticateReason::Tag::fingerprintAuthenticateReason>();
    mSession->authenticateWithContext(2, prompt, &mSignal);
    deliver(authenticated(kUser, 3));
    EXPECT_EQ(mCb->events, (Events{"error 5/0", "success 3"}));
}

TEST_F(SessionTest, ReleasedHeldMatchStopsTheSensor) {
    mSession->authenticateWithContext(1, context(common::DisplayState::NO_UI, true), &mSignal);
    deliver(authenticated(kUser, 3));
    ASSERT_TRUE(waitFor(mModule.authenticates, 2));
    mSession->onContextChanged(context(common::DisplayState::LOCKSCREEN, true));
    EXPECT_EQ(mCb->events, (Events{"success 3"}));
    EXPECT_EQ(mModule.cancels, 1);
}

TEST_F(SessionTest, CancelRacingRearmNeverRestartsTheSensor) {
    // Whichever of the re-arm thread and the cancel runs first, the module is
    // never asked to authenticate after the cancel.
    mModule.cancelReportsCanceled = true;
    mSession->authenticateWithContext(1, context(common::DisplayState::NO_UI, true), &mSignal);
    deliver(authenticated(kUser, 3));
    mSignal->cancel();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::lock_guard lock(mModule.callsMutex);
    auto cancel = std::find(mModule.calls.begin(), mModule.calls.end(), "cancel");
    ASSERT_NE(cancel, mModule.calls.end());
    EXPECT_EQ(std::find(cancel, mModule.calls.end(), "authenticate"), mModule.calls.end());
    EXPECT_EQ(mCb->events, (Events{"error 5/0"}));
}

}  // namespace
}  // namespace aidl::android::hardware::biometrics::fingerprint
