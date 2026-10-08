/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

/**
 * Which audioserver status one slot reports to the radio daemon with setError, as stock does:
 * every change once and in order (STATUS_SERVER_DIED and the following STATUS_OK both matter,
 * because the daemon re-sends the call state on the second), nothing twice in a row, and the
 * current status again after every registration, the first one included. A report the daemon
 * did not receive is sent again, before the current status if that alone would not show the
 * change. Statuses are AudioError values; sequence numbers come from {@link AudioServerMonitor}
 * and make a stale update harmless. Not thread-safe: each slot uses it on its own thread.
 */
final class StatusRelay {
    static final int NONE = -1;

    private int status;
    private long sequence;
    private int lastSent = NONE;
    // A status the daemon did not receive, which differs from the last one it did.
    private int missed = NONE;
    private boolean registered;

    StatusRelay(int status, long sequence) {
        this.status = status;
        this.sequence = sequence;
    }

    /** Returns the status to send now, or NONE. */
    int onAudioStatus(int newStatus, long newSequence) {
        if (newSequence <= sequence) return NONE;
        status = newStatus;
        sequence = newSequence;
        return pending();
    }

    /** Returns the status to send now, or NONE. */
    int onRegistered() {
        registered = true;
        lastSent = NONE;
        missed = NONE;
        return pending();
    }

    void onUnregistered() {
        registered = false;
    }

    void onSent(int sent) {
        lastSent = sent;
        missed = NONE;
    }

    /** setError did not reach the daemon, which is still registered. */
    void onSendFailed(int failed) {
        if (failed != lastSent) missed = failed;
    }

    /**
     * Returns the status to send now, or NONE. If audioserver went back to the status the daemon
     * last received while the change in between was lost, that change goes first: the daemon
     * re-sends the call state on the STATUS_OK that follows STATUS_SERVER_DIED.
     */
    int pending() {
        if (!registered) return NONE;
        return status != lastSent ? status : missed;
    }
}
