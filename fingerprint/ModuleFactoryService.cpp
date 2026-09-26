// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "fp6-fingerprint"

#include "ModuleFactoryService.h"

#include <cstring>

#include <android-base/logging.h>

namespace fp6 {

binder_exception_t addServiceUnlessModuleFactory(AIBinder* binder, const char* instance,
                                                 AddServiceFn addService) {
    if (instance != nullptr && strcmp(instance, kModuleFactoryService) == 0) {
        LOG(INFO) << "Not registering the module's " << kModuleFactoryService;
        return EX_NONE;
    }
    return addService(binder, instance);
}

}  // namespace fp6
