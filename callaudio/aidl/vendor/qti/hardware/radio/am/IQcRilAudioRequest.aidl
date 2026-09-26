/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

// Implemented by the client and called by the radio daemon (see
// IQcRilAudio.aidl). The strings are audio HAL key=value pairs, passed through
// unchanged.

package vendor.qti.hardware.radio.am;

@VintfStability
interface IQcRilAudioRequest {
    // Transaction 1. Keys to read with the global getParameters.
    oneway void queryParameters(in int token, in String params);

    // Transaction 2. Pairs to apply with the global setParameters.
    oneway void setParameters(in int token, in String params);
}
