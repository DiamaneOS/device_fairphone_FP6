// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <android/binder_manager.h>

// The FocalTech module registers a factory/debug binder service,
// FocalFingerprintService, every time it opens, and does not initialise when
// that registration fails. The service's one dispatcher (invokeCommand) has no
// caller check, so it must never reach servicemanager: this process answers the
// module's registration itself (main.cpp) and passes every other service on.
// The service stays unlabelled in SELinux as well (sepolicy/fp6/fingerprint.te).
namespace fp6 {

inline constexpr char kModuleFactoryService[] = "FocalFingerprintService";

using AddServiceFn = binder_exception_t (*)(AIBinder* binder, const char* instance);

// Reports success for the module's factory service without registering it, and
// hands every other service to addService.
binder_exception_t addServiceUnlessModuleFactory(AIBinder* binder, const char* instance,
                                                 AddServiceFn addService);

}  // namespace fp6
