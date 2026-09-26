/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

// Client-side copy of the QCRIL call-audio interface (see IQcRilAudio.aidl).

package vendor.qti.hardware.radio.am;

@VintfStability
@Backing(type="int")
enum AudioError {
    STATUS_OK = 0,
    GENERIC_FAILURE = 1,
    STATUS_SERVER_DIED = 2,
}
