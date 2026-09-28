/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

/**
 * Which audioserver status one slot reports to the radio daemon with setError, as stock does:
 * every change once and in order (STATUS_SERVER_DIED and the following STATUS_OK both matter,
 * because the daemon re-sends the call state on the second), nothing twice in a row, and the
 * current status again after every registration, the first one included. Statuses are
 * AudioError values; sequence numbers come from {@link AudioServerMonitor} and make a stale
 * update harmless. Not thread-safe: each slot uses it on its own thread.
 */
final class StatusRelay {
    static final int NONE = -1;

    private int status;
    private long sequence;
    private int lastSent = NONE;
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
        return pending();
    }

    void onUnregistered() {
        registered = false;
    }

    void onSent(int sent) {
        lastSent = sent;
    }

    private int pending() {
        return registered && status != lastSent ? status : NONE;
    }
}
