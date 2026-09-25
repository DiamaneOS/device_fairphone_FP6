// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "fp6-fingerprint"

#include "Fingerprint.h"

#include <cerrno>
#include <string>
#include <utility>
#include <sys/stat.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "Session.h"

namespace aidl::android::hardware::biometrics::fingerprint {

namespace {

// Kept from the stock FP6 service so existing enrollments stay attached.
constexpr int32_t kSensorId = 5;
constexpr int32_t kMaxEnrollmentsPerUser = 5;

// Side sensor in the power button on the right edge of the 1116x2484 panel.
constexpr int32_t kSensorLocationX = 1116;
constexpr int32_t kSensorLocationY = 1100;
constexpr int32_t kSensorRadius = 100;

constexpr uint16_t kModuleVersion = HARDWARE_MODULE_API_VERSION(2, 1);

Fingerprint* sInstance = nullptr;

}  // namespace

std::shared_ptr<Fingerprint> Fingerprint::create() {
    const hw_module_t* module = nullptr;
    if (int err = hw_get_module(FINGERPRINT_HARDWARE_MODULE_ID, &module); err != 0) {
        LOG(ERROR) << "can't load fingerprint module: " << err;
        return nullptr;
    }
    hw_device_t* device = nullptr;
    if (module->methods->open == nullptr ||
        module->methods->open(module, nullptr, &device) != 0 || device == nullptr) {
        LOG(ERROR) << "can't open fingerprint module";
        return nullptr;
    }
    if (device->version != kModuleVersion) {
        LOG(ERROR) << "unexpected fingerprint module version " << device->version;
        return nullptr;
    }
    auto fpDevice = reinterpret_cast<fingerprint_device_t*>(device);
    auto hal = ndk::SharedRefBase::make<Fingerprint>(fpDevice);
    sInstance = hal.get();
    if (int err = fpDevice->set_notify(fpDevice, &Fingerprint::notify); err != 0) {
        LOG(ERROR) << "can't register fingerprint callback: " << err;
        sInstance = nullptr;
        return nullptr;
    }
    return hal;
}

Fingerprint::Fingerprint(fingerprint_device_t* device) : mDevice(device) {}

ndk::ScopedAStatus Fingerprint::getSensorProps(std::vector<SensorProps>* out) {
    SensorProps props;
    props.commonProps.sensorId = kSensorId;
    props.commonProps.sensorStrength = common::SensorStrength::STRONG;
    props.commonProps.maxEnrollmentsPerUser = kMaxEnrollmentsPerUser;
    props.sensorType = FingerprintSensorType::POWER_BUTTON;
    SensorLocation location;
    location.sensorLocationX = kSensorLocationX;
    location.sensorLocationY = kSensorLocationY;
    location.sensorRadius = kSensorRadius;
    props.sensorLocations.push_back(location);
    props.supportsNavigationGestures = false;
    props.supportsDetectInteraction = true;
    props.halHandlesDisplayTouches = false;
    props.halControlsIllumination = false;
    *out = {std::move(props)};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Fingerprint::createSession(int32_t sensorId, int32_t userId,
                                              const std::shared_ptr<ISessionCallback>& cb,
                                              std::shared_ptr<ISession>* out) {
    if (sensorId != kSensorId || cb == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    // Templates live in the user's device-encrypted vendor storage.
    const std::string path = ::android::base::StringPrintf("/data/vendor_de/%d/fpdata", userId);
    if (mkdir(path.c_str(), 0700) != 0 && errno != EEXIST) {
        PLOG(ERROR) << "can't create " << path;
    }
    {
        std::lock_guard moduleLock(moduleMutex());
        if (int err = mDevice->set_active_group(mDevice, userId, path.c_str()); err != 0) {
            LOG(ERROR) << "set_active_group(" << userId << "): " << err;
            return ndk::ScopedAStatus::fromServiceSpecificError(err);
        }
    }
    auto session = ndk::SharedRefBase::make<Session>(mDevice, userId, cb, [this](const Session* s) {
        std::lock_guard lock(mSessionMutex);
        if (mSession.get() == s) mSession.reset();
    });
    session->watchClient();
    std::shared_ptr<Session> previous;
    {
        std::lock_guard lock(mSessionMutex);
        previous = std::exchange(mSession, session);
    }
    // The framework closes a session before opening the next; if it did not,
    // the old one must not receive this session's messages or cancel its work.
    if (previous != nullptr) previous->detach(false);
    *out = session;
    return ndk::ScopedAStatus::ok();
}

void Fingerprint::notify(const fingerprint_msg_t* msg) {
    if (sInstance == nullptr || msg == nullptr) return;
    std::shared_ptr<Session> session;
    {
        std::lock_guard lock(sInstance->mSessionMutex);
        session = sInstance->mSession;
    }
    if (session == nullptr) {
        LOG(WARNING) << "module message " << msg->type << " without an open session";
        return;
    }
    session->onMessage(msg);
}

}  // namespace aidl::android::hardware::biometrics::fingerprint
