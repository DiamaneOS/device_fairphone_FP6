/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

// Client-side copy of the frozen version 1 of the QCRIL call-audio interface
// that the stock radio daemon serves as IQcRilAudio/slot1 and /slot2. Names,
// method order (transaction codes), oneway flags, version and hash were read
// from the stock QtiTelephonyService stubs and checked against the stock
// vendor.qti.hardware.radio.am-V1-ndk.so. The build passes version 1 and the
// hash to the compiler (Android.bp); do not reorder or change anything here.

package vendor.qti.hardware.radio.am;

import vendor.qti.hardware.radio.am.AudioError;
import vendor.qti.hardware.radio.am.IQcRilAudioRequest;
import vendor.qti.hardware.radio.am.IQcRilAudioResponse;

@VintfStability
interface IQcRilAudio {
    // Transaction 1. Registers the client's request callback; returns the
    // radio daemon's response callback.
    IQcRilAudioResponse setRequestInterface(in IQcRilAudioRequest request);

    // Transaction 2. Audio server state: STATUS_OK, GENERIC_FAILURE (not
    // running at start) or STATUS_SERVER_DIED.
    oneway void setError(in AudioError error);
}
