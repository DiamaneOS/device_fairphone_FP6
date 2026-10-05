// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Power stats HAL for the FP6 (PowerStats.h). One binder thread: requests are
// handled one at a time.

#include <stdlib.h>

#include <memory>
#include <string>

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "PowerStats.h"

using aidl::android::hardware::power::stats::PowerStats;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<PowerStats> powerStats = ndk::SharedRefBase::make<PowerStats>();

    const std::string instance = std::string(PowerStats::descriptor) + "/default";
    const binder_status_t status =
            AServiceManager_addService(powerStats->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK) << "Cannot register " << instance;

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // joinThreadPool does not return
}
