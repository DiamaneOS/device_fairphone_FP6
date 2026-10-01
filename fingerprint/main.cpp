// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "fp6-fingerprint"

#include <dlfcn.h>

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "Fingerprint.h"
#include "ModuleFactoryService.h"

using aidl::android::hardware::biometrics::fingerprint::Fingerprint;

// Exported from this executable (exported_symbols.list), so the FocalTech
// module, loaded later by hw_get_module, binds its registration to this
// definition rather than libbinder_ndk's and its factory service stays in this
// process (ModuleFactoryService.h). Every other service reaches libbinder_ndk.
extern "C" binder_exception_t AServiceManager_addService(AIBinder* binder, const char* instance) {
    static const auto next =
            reinterpret_cast<fp6::AddServiceFn>(dlsym(RTLD_NEXT, "AServiceManager_addService"));
    CHECK(next != nullptr) << "libbinder_ndk's AServiceManager_addService not found";
    return fp6::addServiceUnlessModuleFactory(binder, instance, next);
}

int main() {
    // No extra pool threads of our own. The FocalTech module starts a service
    // thread that joins the binder pool as well (ISession calls arrive on
    // it), so requests can run concurrently; Session serialises every module
    // call (moduleMutex).
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<Fingerprint> hal = Fingerprint::create();
    CHECK(hal != nullptr) << "FP6 fingerprint module unavailable";
    const std::string instance = std::string(Fingerprint::descriptor) + "/default";
    CHECK_EQ(AServiceManager_addService(hal->asBinder().get(), instance.c_str()), STATUS_OK);
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not be reached
}
