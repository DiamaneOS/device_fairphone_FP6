// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "fp6-fingerprint"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "Fingerprint.h"

using aidl::android::hardware::biometrics::fingerprint::Fingerprint;

int main() {
    // One binder thread: requests reach the module one at a time.
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<Fingerprint> hal = Fingerprint::create();
    CHECK(hal != nullptr) << "FP6 fingerprint module unavailable";
    const std::string instance = std::string(Fingerprint::descriptor) + "/default";
    CHECK_EQ(AServiceManager_addService(hal->asBinder().get(), instance.c_str()), STATUS_OK);
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not be reached
}
