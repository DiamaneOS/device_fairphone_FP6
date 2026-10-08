/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public final class StatusRelayTest {
    // vendor.qti.hardware.radio.am.AudioError values.
    private static final int OK = 0;
    private static final int GENERIC_FAILURE = 1;
    private static final int SERVER_DIED = 2;
    private static final int NONE = StatusRelay.NONE;

    private static int sendIfAny(StatusRelay relay, int status) {
        if (status != NONE) relay.onSent(status);
        return status;
    }

    @Test
    public void firstRegistrationAlwaysReports() {
        StatusRelay relay = new StatusRelay(OK, 0);
        assertEquals(OK, sendIfAny(relay, relay.onRegistered()));
    }

    @Test
    public void reportsNotRunningAtStart() {
        StatusRelay relay = new StatusRelay(GENERIC_FAILURE, 0);
        assertEquals(GENERIC_FAILURE, sendIfAny(relay, relay.onRegistered()));
        assertEquals(OK, sendIfAny(relay, relay.onAudioStatus(OK, 1)));
    }

    @Test
    public void nothingBeforeRegistration() {
        StatusRelay relay = new StatusRelay(OK, 0);
        assertEquals(NONE, relay.onAudioStatus(SERVER_DIED, 1));
        assertEquals(SERVER_DIED, sendIfAny(relay, relay.onRegistered()));
    }

    @Test
    public void restartSendsDiedThenOk() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        assertEquals(SERVER_DIED, sendIfAny(relay, relay.onAudioStatus(SERVER_DIED, 1)));
        assertEquals(OK, sendIfAny(relay, relay.onAudioStatus(OK, 2)));
    }

    @Test
    public void noRepeats() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        assertEquals(NONE, relay.onAudioStatus(OK, 1));
    }

    @Test
    public void staleUpdatesIgnored() {
        StatusRelay relay = new StatusRelay(OK, 5);
        sendIfAny(relay, relay.onRegistered());
        assertEquals(NONE, relay.onAudioStatus(SERVER_DIED, 5));
        assertEquals(NONE, relay.onAudioStatus(SERVER_DIED, 3));
        assertEquals(SERVER_DIED, sendIfAny(relay, relay.onAudioStatus(SERVER_DIED, 6)));
    }

    @Test
    public void reRegistrationReportsCurrentStatusAgain() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        relay.onUnregistered();
        assertEquals(NONE, relay.onAudioStatus(SERVER_DIED, 1));
        assertEquals(NONE, relay.onAudioStatus(OK, 2));
        assertEquals(OK, sendIfAny(relay, relay.onRegistered()));
    }

    /**
     * The radio daemon restarts and the slot thread blocks until it registers again; meanwhile
     * audioserver dies and the update waits in the slot's queue. Before reporting on the new
     * registration the slot takes the monitor's current status, so the daemon is not told that
     * audio is up while it is down, and the queued update then changes nothing.
     */
    @Test
    public void reRegistrationReportsCurrentStatusNotQueuedOne() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        relay.onUnregistered();
        // connect(): snapshot, then registration.
        assertEquals(NONE, relay.onAudioStatus(SERVER_DIED, 1));
        assertEquals(SERVER_DIED, sendIfAny(relay, relay.onRegistered()));
        // The queued update, then audioserver back.
        assertEquals(NONE, relay.onAudioStatus(SERVER_DIED, 1));
        assertEquals(OK, sendIfAny(relay, relay.onAudioStatus(OK, 2)));
    }

    @Test
    public void reRegistrationAfterARestartDropsBothQueuedUpdates() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        relay.onUnregistered();
        // audioserver died and came back while the thread was blocked; the snapshot is OK 2.
        assertEquals(NONE, relay.onAudioStatus(OK, 2));
        assertEquals(OK, sendIfAny(relay, relay.onRegistered()));
        assertEquals(NONE, relay.onAudioStatus(SERVER_DIED, 1));
        assertEquals(NONE, relay.onAudioStatus(OK, 2));
    }

    /** A oneway setError can fail while the daemon lives (full binder buffer): no death follows. */
    @Test
    public void lostReportIsSentAgain() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        assertEquals(SERVER_DIED, relay.onAudioStatus(SERVER_DIED, 1));
        relay.onSendFailed(SERVER_DIED);
        assertEquals(SERVER_DIED, sendIfAny(relay, relay.pending()));
        assertEquals(NONE, relay.pending());
    }

    /** audioserver came back before the retry: the daemon still gets the death, then OK. */
    @Test
    public void lostDeathGoesBeforeTheRecovery() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        relay.onSendFailed(relay.onAudioStatus(SERVER_DIED, 1));
        assertEquals(SERVER_DIED, sendIfAny(relay, relay.onAudioStatus(OK, 2)));
        assertEquals(OK, sendIfAny(relay, relay.pending()));
        assertEquals(NONE, relay.pending());
    }

    @Test
    public void lostRecoveryIsSentAgain() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        sendIfAny(relay, relay.onAudioStatus(SERVER_DIED, 1));
        relay.onSendFailed(relay.onAudioStatus(OK, 2));
        assertEquals(OK, sendIfAny(relay, relay.pending()));
        assertEquals(NONE, relay.pending());
    }

    @Test
    public void newRegistrationStartsFromTheCurrentStatus() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        relay.onSendFailed(relay.onAudioStatus(SERVER_DIED, 1));
        relay.onAudioStatus(OK, 2);
        relay.onUnregistered();
        assertEquals(NONE, relay.pending());
        assertEquals(OK, sendIfAny(relay, relay.onRegistered()));
        assertEquals(NONE, relay.pending());
    }

    @Test
    public void unsentStatusIsRetriedOnNextChange() {
        StatusRelay relay = new StatusRelay(OK, 0);
        sendIfAny(relay, relay.onRegistered());
        // The send failed (daemon dying): nothing recorded, so the next registration reports.
        assertEquals(SERVER_DIED, relay.onAudioStatus(SERVER_DIED, 1));
        relay.onUnregistered();
        assertEquals(SERVER_DIED, sendIfAny(relay, relay.onRegistered()));
    }
}
