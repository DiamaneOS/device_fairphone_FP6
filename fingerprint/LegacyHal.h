// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <cstddef>
#include <cstdint>

#include <hardware/fingerprint.h>
#include <hardware/hw_auth_token.h>

// The FP6 FocalTech module (libfingerprint.default.so) implements the legacy
// fingerprint_device_t interface plus three extensions in the reserved slots,
// and sends two extra message types. The layout matches what the stock FP6
// fingerprint service expects from the same module.
namespace fp6 {

using InvalidateAuthenticatorIdFn = int (*)(fingerprint_device_t* dev, uint64_t* newId);
using ResetLockoutFn = int (*)(fingerprint_device_t* dev, const hw_auth_token_t* hat);
using DetectInteractionFn = int (*)(fingerprint_device_t* dev);

enum ReservedSlot : size_t {
    kInvalidateAuthenticatorId = 0,
    kResetLockout = 1,
    kDetectInteraction = 2,
};

// Extra fingerprint_msg_t types.
constexpr int kMsgLockout = 7;
constexpr int kMsgInteractionDetected = 8;

// Payload of kMsgLockout, stored at fingerprint_msg_t::data.
struct LockoutMsg {
    uint32_t kind;
    uint32_t reserved;
    int64_t durationMillis;
};
enum LockoutKind : uint32_t {
    kLockoutTimed = 0,
    kLockoutPermanent = 1,
    kLockoutCleared = 2,
};

static_assert(offsetof(fingerprint_msg_t, data) == 8);
static_assert(sizeof(LockoutMsg) <= sizeof(fingerprint_msg_t::data));
static_assert(sizeof(hw_auth_token_t) == 69);

template <typename Fn>
Fn reservedOp(fingerprint_device_t* dev, ReservedSlot slot) {
    return reinterpret_cast<Fn>(dev->reserved[slot]);
}

}  // namespace fp6
