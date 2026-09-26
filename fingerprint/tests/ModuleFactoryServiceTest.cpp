// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include <gtest/gtest.h>

#include "ModuleFactoryService.h"

namespace fp6 {
namespace {

int gCalls;
AIBinder* gBinder;
const char* gInstance;

binder_exception_t recordingAddService(AIBinder* binder, const char* instance) {
    ++gCalls;
    gBinder = binder;
    gInstance = instance;
    return EX_ILLEGAL_STATE;
}

class ModuleFactoryServiceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        gCalls = 0;
        gBinder = nullptr;
        gInstance = nullptr;
    }
};

TEST_F(ModuleFactoryServiceTest, KeepsTheModuleFactoryServiceUnregistered) {
    EXPECT_EQ(addServiceUnlessModuleFactory(nullptr, "FocalFingerprintService",
                                            recordingAddService),
              EX_NONE);
    EXPECT_EQ(gCalls, 0);
}

TEST_F(ModuleFactoryServiceTest, PassesOtherServicesOn) {
    // Only compared, never dereferenced.
    AIBinder* binder = reinterpret_cast<AIBinder*>(0x1000);
    const char* instance = "android.hardware.biometrics.fingerprint.IFingerprint/default";
    EXPECT_EQ(addServiceUnlessModuleFactory(binder, instance, recordingAddService),
              EX_ILLEGAL_STATE);
    EXPECT_EQ(gCalls, 1);
    EXPECT_EQ(gBinder, binder);
    EXPECT_STREQ(gInstance, instance);
}

TEST_F(ModuleFactoryServiceTest, MatchesTheExactNameOnly) {
    for (const char* instance : {"FocalFingerprintService2", "focalfingerprintservice",
                                 "FocalFingerprint", ""}) {
        SetUp();
        EXPECT_EQ(addServiceUnlessModuleFactory(nullptr, instance, recordingAddService),
                  EX_ILLEGAL_STATE)
                << instance;
        EXPECT_EQ(gCalls, 1) << instance;
    }
}

TEST_F(ModuleFactoryServiceTest, PassesANullNameOn) {
    EXPECT_EQ(addServiceUnlessModuleFactory(nullptr, nullptr, recordingAddService),
              EX_ILLEGAL_STATE);
    EXPECT_EQ(gCalls, 1);
    EXPECT_EQ(gInstance, nullptr);
}

}  // namespace
}  // namespace fp6
