// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <android/binder_parcel_utils.h>

// Reader for the parcelable SystemUI attaches to fingerprint authentication
// requests to carry the "Touch to unlock anytime" setting. Its AIDL definition
// (frameworks/base/packages/SystemUI/google/PressToAuthParcelable) has no NDK
// backend, so this reads its structured-parcelable layout directly: a size
// header followed by one boolean.
namespace fp6 {

struct PressToAuth {
    static constexpr const char* descriptor =
            "com.google.hardware.biometrics.parcelables.fingerprint.PressToAuthParcelable";

    // True when "Touch to unlock anytime" is off: a touch must not unlock
    // while the screen is off, only after the power button turns it on.
    bool pressToAuthEnabled = false;

    binder_status_t readFromParcel(const AParcel* parcel) {
        int32_t start = AParcel_getDataPosition(parcel);
        int32_t size = 0;
        if (binder_status_t status = AParcel_readInt32(parcel, &size); status != STATUS_OK) {
            return status;
        }
        if (size < 4) return STATUS_BAD_VALUE;
        if (AParcel_getDataPosition(parcel) - start < size) {
            if (binder_status_t status = AParcel_readBool(parcel, &pressToAuthEnabled);
                status != STATUS_OK) {
                return status;
            }
        }
        return AParcel_setDataPosition(parcel, start + size);
    }

    binder_status_t writeToParcel(AParcel*) const { return STATUS_INVALID_OPERATION; }
};

}  // namespace fp6
