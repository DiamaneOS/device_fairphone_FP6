// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "fp6-fingerprint"

#include "Session.h"

#include <algorithm>
#include <cstring>
#include <endian.h>

#include <aidl/android/hardware/biometrics/common/BnCancellationSignal.h>
#include <android-base/logging.h>

namespace aidl::android::hardware::biometrics::fingerprint {

namespace {

// Same enrollment timeout the framework used with legacy modules.
constexpr uint32_t kEnrollTimeoutSec = 60;

class CancellationSignal : public common::BnCancellationSignal {
  public:
    explicit CancellationSignal(fingerprint_device_t* device) : mDevice(device) {}
    ndk::ScopedAStatus cancel() override {
        if (int err = mDevice->cancel(mDevice); err != 0) LOG(ERROR) << "cancel failed: " << err;
        return ndk::ScopedAStatus::ok();
    }

  private:
    fingerprint_device_t* mDevice;
};

// hw_auth_token_t keeps authenticator_type and timestamp in network byte order.
hw_auth_token_t toLegacy(const HardwareAuthToken& hat) {
    hw_auth_token_t token = {};
    token.version = HW_AUTH_TOKEN_VERSION;
    token.challenge = hat.challenge;
    token.user_id = hat.userId;
    token.authenticator_id = hat.authenticatorId;
    token.authenticator_type = htobe32(static_cast<uint32_t>(hat.authenticatorType));
    token.timestamp = htobe64(static_cast<uint64_t>(hat.timestamp.milliSeconds));
    std::memcpy(token.hmac, hat.mac.data(), std::min(hat.mac.size(), sizeof(token.hmac)));
    return token;
}

HardwareAuthToken fromLegacy(const hw_auth_token_t& token) {
    HardwareAuthToken hat;
    hat.challenge = token.challenge;
    hat.userId = token.user_id;
    hat.authenticatorId = token.authenticator_id;
    hat.authenticatorType =
            static_cast<keymaster::HardwareAuthenticatorType>(be32toh(token.authenticator_type));
    hat.timestamp.milliSeconds = static_cast<int64_t>(be64toh(token.timestamp));
    hat.mac.assign(token.hmac, token.hmac + sizeof(token.hmac));
    return hat;
}

Error toAidlError(int32_t error, int32_t* vendorCode) {
    *vendorCode = 0;
    if (error >= FINGERPRINT_ERROR_HW_UNAVAILABLE && error <= FINGERPRINT_ERROR_UNABLE_TO_REMOVE)
        return static_cast<Error>(error);
    if (error >= FINGERPRINT_ERROR_VENDOR_BASE) {
        *vendorCode = error - FINGERPRINT_ERROR_VENDOR_BASE;
        return Error::VENDOR;
    }
    // Lockout is reported separately through the module's lockout messages.
    LOG(WARNING) << "unexpected module error " << error;
    return Error::UNABLE_TO_PROCESS;
}

AcquiredInfo toAidlAcquired(int32_t info, int32_t* vendorCode) {
    *vendorCode = 0;
    if (info >= FINGERPRINT_ACQUIRED_GOOD && info <= FINGERPRINT_ACQUIRED_TOO_FAST)
        return static_cast<AcquiredInfo>(info + 1);
    if (info >= FINGERPRINT_ACQUIRED_VENDOR_BASE) {
        *vendorCode = info - FINGERPRINT_ACQUIRED_VENDOR_BASE;
        return AcquiredInfo::VENDOR;
    }
    return AcquiredInfo::INSUFFICIENT;
}

}  // namespace

Session::Session(fingerprint_device_t* device, int32_t userId,
                 std::shared_ptr<ISessionCallback> cb)
    : mDevice(device), mUserId(userId), mCb(std::move(cb)) {}

std::shared_ptr<ICancellationSignal> Session::cancellationSignal() {
    return ndk::SharedRefBase::make<CancellationSignal>(mDevice);
}

void Session::reportError(Error error) {
    mCb->onError(error, 0);
}

void Session::onMessage(const fingerprint_msg_t* msg) {
    int32_t vendorCode = 0;
    switch (static_cast<int>(msg->type)) {
        case FINGERPRINT_ERROR: {
            Error error = toAidlError(msg->data.error, &vendorCode);
            mCb->onError(error, vendorCode);
            break;
        }
        case FINGERPRINT_ACQUIRED: {
            AcquiredInfo info = toAidlAcquired(msg->data.acquired.acquired_info, &vendorCode);
            mCb->onAcquired(info, vendorCode);
            break;
        }
        case FINGERPRINT_TEMPLATE_ENROLLING:
            mCb->onEnrollmentProgress(msg->data.enroll.finger.fid,
                                      msg->data.enroll.samples_remaining);
            break;
        case FINGERPRINT_TEMPLATE_REMOVED:
            if (msg->data.removed.finger.fid != 0) {
                std::lock_guard lock(mMutex);
                mRemoved.push_back(msg->data.removed.finger.fid);
            }
            break;
        case FINGERPRINT_AUTHENTICATED:
            if (msg->data.authenticated.finger.fid != 0) {
                mCb->onAuthenticationSucceeded(msg->data.authenticated.finger.fid,
                                               fromLegacy(msg->data.authenticated.hat));
            } else {
                mCb->onAuthenticationFailed();
            }
            break;
        case FINGERPRINT_TEMPLATE_ENUMERATING: {
            std::vector<int32_t> ids;
            {
                std::lock_guard lock(mMutex);
                if (msg->data.enumerated.finger.fid != 0)
                    mEnumerated.push_back(msg->data.enumerated.finger.fid);
                if (msg->data.enumerated.remaining_templates != 0) break;
                ids.swap(mEnumerated);
            }
            mCb->onEnrollmentsEnumerated(ids);
            break;
        }
        case fp6::kMsgLockout: {
            fp6::LockoutMsg lockout;
            std::memcpy(&lockout, &msg->data, sizeof(lockout));
            switch (lockout.kind) {
                case fp6::kLockoutTimed:
                    mCb->onLockoutTimed(lockout.durationMillis);
                    break;
                case fp6::kLockoutPermanent:
                    mCb->onLockoutPermanent();
                    break;
                case fp6::kLockoutCleared:
                    mCb->onLockoutCleared();
                    break;
                default:
                    LOG(WARNING) << "unknown lockout message " << lockout.kind;
            }
            break;
        }
        case fp6::kMsgInteractionDetected:
            mCb->onInteractionDetected();
            break;
        default:
            LOG(WARNING) << "unknown module message " << msg->type;
    }
}

ndk::ScopedAStatus Session::generateChallenge() {
    mCb->onChallengeGenerated(static_cast<int64_t>(mDevice->pre_enroll(mDevice)));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::revokeChallenge(int64_t challenge) {
    if (int err = mDevice->post_enroll(mDevice); err != 0) LOG(ERROR) << "post_enroll: " << err;
    mCb->onChallengeRevoked(challenge);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enroll(const HardwareAuthToken& hat,
                                   std::shared_ptr<ICancellationSignal>* out) {
    // The trusted app verifies the HAT before it starts enrolling.
    hw_auth_token_t token = toLegacy(hat);
    if (int err = mDevice->enroll(mDevice, &token, mUserId, kEnrollTimeoutSec); err != 0) {
        LOG(ERROR) << "enroll: " << err;
        reportError(Error::UNABLE_TO_PROCESS);
    }
    *out = cancellationSignal();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::authenticate(int64_t operationId,
                                         std::shared_ptr<ICancellationSignal>* out) {
    if (int err = mDevice->authenticate(mDevice, operationId, mUserId); err != 0) {
        LOG(ERROR) << "authenticate: " << err;
        reportError(Error::UNABLE_TO_PROCESS);
    }
    *out = cancellationSignal();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::detectInteraction(std::shared_ptr<ICancellationSignal>* out) {
    auto detect = fp6::reservedOp<fp6::DetectInteractionFn>(mDevice, fp6::kDetectInteraction);
    if (detect == nullptr) {
        reportError(Error::UNABLE_TO_PROCESS);
    } else if (int err = detect(mDevice); err != 0) {
        LOG(ERROR) << "detectInteraction: " << err;
        reportError(Error::UNABLE_TO_PROCESS);
    }
    *out = cancellationSignal();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enumerateEnrollments() {
    {
        std::lock_guard lock(mMutex);
        mEnumerated.clear();
    }
    if (int err = mDevice->enumerate(mDevice); err != 0) {
        LOG(ERROR) << "enumerate: " << err;
        reportError(Error::UNABLE_TO_PROCESS);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::removeEnrollments(const std::vector<int32_t>& enrollmentIds) {
    {
        std::lock_guard lock(mMutex);
        mRemoved.clear();
    }
    // The module reports each removal before remove() returns.
    for (int32_t id : enrollmentIds) {
        if (int err = mDevice->remove(mDevice, mUserId, id); err != 0)
            LOG(ERROR) << "remove(" << id << "): " << err;
    }
    std::vector<int32_t> removed;
    {
        std::lock_guard lock(mMutex);
        removed.swap(mRemoved);
    }
    mCb->onEnrollmentsRemoved(removed);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::getAuthenticatorId() {
    mCb->onAuthenticatorIdRetrieved(static_cast<int64_t>(mDevice->get_authenticator_id(mDevice)));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::invalidateAuthenticatorId() {
    auto invalidate = fp6::reservedOp<fp6::InvalidateAuthenticatorIdFn>(
            mDevice, fp6::kInvalidateAuthenticatorId);
    uint64_t id = 0;
    if (invalidate == nullptr || invalidate(mDevice, &id) != 0) {
        LOG(ERROR) << "invalidateAuthenticatorId failed";
        reportError(Error::UNABLE_TO_PROCESS);
        return ndk::ScopedAStatus::ok();
    }
    mCb->onAuthenticatorIdInvalidated(static_cast<int64_t>(id));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::resetLockout(const HardwareAuthToken& hat) {
    // The trusted app checks the HAT and reports onLockoutCleared itself.
    auto reset = fp6::reservedOp<fp6::ResetLockoutFn>(mDevice, fp6::kResetLockout);
    hw_auth_token_t token = toLegacy(hat);
    if (reset == nullptr || reset(mDevice, &token) != 0) {
        LOG(ERROR) << "resetLockout rejected";
        reportError(Error::UNABLE_TO_PROCESS);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::close() {
    mCb->onSessionClosed();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerDown(int32_t, int32_t, int32_t, float, float) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerUp(int32_t) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onUiReady() {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::authenticateWithContext(int64_t operationId, const OperationContext&,
                                                    std::shared_ptr<ICancellationSignal>* out) {
    return authenticate(operationId, out);
}

ndk::ScopedAStatus Session::enrollWithContext(const HardwareAuthToken& hat,
                                              const OperationContext&,
                                              std::shared_ptr<ICancellationSignal>* out) {
    return enroll(hat, out);
}

ndk::ScopedAStatus Session::detectInteractionWithContext(
        const OperationContext&, std::shared_ptr<ICancellationSignal>* out) {
    return detectInteraction(out);
}

ndk::ScopedAStatus Session::onPointerDownWithContext(const PointerContext&) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerUpWithContext(const PointerContext&) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onContextChanged(const OperationContext&) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerCancelWithContext(const PointerContext&) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::setIgnoreDisplayTouches(bool) {
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::biometrics::fingerprint
