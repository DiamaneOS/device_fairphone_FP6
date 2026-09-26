/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

// Implemented by the radio daemon and returned by setRequestInterface (see
// IQcRilAudio.aidl). Each reply carries the token of its request.

package vendor.qti.hardware.radio.am;

import vendor.qti.hardware.radio.am.AudioError;

@VintfStability
interface IQcRilAudioResponse {
    // Transaction 1.
    oneway void queryParametersResponse(in int token, in String params);

    // Transaction 2.
    oneway void setParametersResponse(in int token, in AudioError error);
}
