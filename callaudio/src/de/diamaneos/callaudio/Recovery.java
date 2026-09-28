/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

import java.util.function.BooleanSupplier;
import java.util.function.IntSupplier;
import java.util.function.LongConsumer;

/**
 * The retry window after audioserver comes back. A new audioserver rejects setParameters until
 * system_server has sent it the permission table again, which system_server starts when
 * media.audio_policy registers, at the same moment the bridge sees audioserver back and tells
 * the radio daemon, which then re-sends the call state. Inside the window a rejected
 * setParameters is retried; the window closes after {@link #WINDOW_MS} or when audioserver dies
 * again. Outside it nothing is retried, as stock. Times are elapsed-realtime milliseconds.
 */
final class Recovery {
    static final long WINDOW_MS = 10_000;
    static final long RETRY_MS = 100;
    /** AudioSystem.SUCCESS. */
    static final int SUCCESS = 0;

    private long start = -1; // Guarded by this.

    synchronized void begin(long now) {
        start = now;
    }

    synchronized void end() {
        start = -1;
    }

    synchronized boolean active(long now) {
        return start >= 0 && now - start < WINDOW_MS;
    }

    /**
     * Runs {@code set}, which returns an AudioSystem status, and while {@code retryAllowed} holds
     * runs it again every {@link #RETRY_MS} until it succeeds; returns the last status. Only the
     * window bounds the retries. Another slot's accepted call must not end them: its request can
     * reach AudioFlinger just after the permission table and this one just before, and the radio
     * daemon does not send an unchanged call state again. AudioFlinger accepts global parameters
     * if any HAL module does, so a HAL rejection that keeps retrying to the end of the window is
     * rare; outside the window every request is tried once.
     */
    static int retry(IntSupplier set, BooleanSupplier retryAllowed, LongConsumer sleep) {
        int status = set.getAsInt();
        while (status != SUCCESS && retryAllowed.getAsBoolean()) {
            sleep.accept(RETRY_MS);
            // Checked again after the sleep: audioserver may have died meanwhile.
            if (retryAllowed.getAsBoolean()) status = set.getAsInt();
        }
        return status;
    }
}
