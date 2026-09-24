// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include <string>
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
    int cancels = 0;
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
int fakeAuthenticate(fingerprint_device_t*, uint64_t, uint32_t) { return finish(); }
int fakeEnumerate(fingerprint_device_t*) { return finish(); }
int fakeResetLockout(fingerprint_device_t*, const hw_auth_token_t*) { return finish(); }
int fakeCancel(fingerprint_device_t*) {
    gModule->cancels++;
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

}  // namespace
}  // namespace aidl::android::hardware::biometrics::fingerprint
