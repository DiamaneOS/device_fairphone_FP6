/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package org.diamaneos.callaudio;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.util.function.BooleanSupplier;
import java.util.function.IntSupplier;
import java.util.function.LongConsumer;

public final class RecoveryTest {
    private static final int DENIED = -1; // AudioSystem.ERROR, as AudioFlinger's refusal arrives.

    @Test
    public void closedUntilARestart() {
        assertFalse(new Recovery().active(1000));
    }

    @Test
    public void openForTheWindowAfterARestart() {
        Recovery recovery = new Recovery();
        recovery.begin(1000);
        assertTrue(recovery.active(1000));
        assertTrue(recovery.active(1000 + Recovery.WINDOW_MS - 1));
        assertFalse(recovery.active(1000 + Recovery.WINDOW_MS));
    }

    @Test
    public void deathClosesIt() {
        Recovery recovery = new Recovery();
        recovery.begin(1000);
        recovery.end();
        assertFalse(recovery.active(1001));
    }

    @Test
    public void anotherRestartReopensIt() {
        Recovery recovery = new Recovery();
        recovery.begin(1000);
        recovery.end();
        recovery.begin(50_000);
        assertTrue(recovery.active(50_001));
    }

    /** A fake AudioFlinger that rejects everything until the permission table arrives. */
    private static final class Flinger {
        final long[] clock = {0};
        long permittedAt = Long.MAX_VALUE;
        int calls;

        IntSupplier set() {
            return () -> {
                calls++;
                return clock[0] >= permittedAt ? Recovery.SUCCESS : DENIED;
            };
        }

        LongConsumer sleep() {
            return ms -> clock[0] += ms;
        }

        BooleanSupplier allowed(Recovery recovery) {
            return () -> recovery.active(clock[0]);
        }

        int retry(Recovery recovery) {
            return Recovery.retry(set(), allowed(recovery), sleep());
        }
    }

    @Test
    public void outsideTheWindowTriedOnceAsStock() {
        Flinger flinger = new Flinger();
        Recovery recovery = new Recovery();
        assertEquals(DENIED, flinger.retry(recovery));
        assertEquals(1, flinger.calls);
        assertEquals(0, flinger.clock[0]);
    }

    @Test
    public void acceptedFirstTimeNoRetry() {
        Flinger flinger = new Flinger();
        flinger.permittedAt = 0;
        Recovery recovery = new Recovery();
        recovery.begin(0);
        assertEquals(Recovery.SUCCESS, flinger.retry(recovery));
        assertEquals(1, flinger.calls);
    }

    /**
     * Two slots re-send their call state within a few milliseconds of audioserver coming back:
     * slot 1's request lands just before system_server's permission push and is denied, slot 2's
     * just after and is accepted. Slot 1 must still be retried; the radio daemon will not send its
     * unchanged call state again.
     */
    @Test
    public void otherSlotAcceptedDoesNotEndRetries() {
        Flinger flinger = new Flinger();
        Recovery recovery = new Recovery();
        recovery.begin(0);
        int[] slot2 = {Integer.MIN_VALUE};
        int[] slot1Calls = {0};
        IntSupplier slot1 = () -> {
            int status = flinger.set().getAsInt();
            if (++slot1Calls[0] == 1) {
                // The permission push lands, then slot 2's request (on its own thread in the
                // app; here in between slot 1's first attempt and its first retry check).
                flinger.permittedAt = flinger.clock[0];
                slot2[0] = flinger.retry(recovery);
            }
            return status;
        };
        int status1 = Recovery.retry(slot1, flinger.allowed(recovery), flinger.sleep());
        assertEquals(Recovery.SUCCESS, slot2[0]);
        assertEquals(Recovery.SUCCESS, status1);
        assertEquals(2, slot1Calls[0]);
        assertEquals(Recovery.RETRY_MS, flinger.clock[0]);
    }

    /** The permission push can take longer than a few retries; the window bounds them. */
    @Test
    public void latePermissionPushStillApplied() {
        Flinger flinger = new Flinger();
        flinger.permittedAt = 3_000; // Beyond the old cap of 20 retries of 100 ms.
        Recovery recovery = new Recovery();
        recovery.begin(0);
        assertEquals(Recovery.SUCCESS, flinger.retry(recovery));
        assertEquals(3_000, flinger.clock[0]);
        assertEquals(31, flinger.calls);
    }

    @Test
    public void stopsWhenTheWindowCloses() {
        Flinger flinger = new Flinger();
        Recovery recovery = new Recovery();
        recovery.begin(0);
        assertEquals(DENIED, flinger.retry(recovery));
        // Attempts at 0, 100, ..., WINDOW_MS - 100; none after the window closed.
        assertEquals(Recovery.WINDOW_MS / Recovery.RETRY_MS, flinger.calls);
        assertEquals(Recovery.WINDOW_MS, flinger.clock[0]);
    }

    @Test
    public void stopsWhenAudioserverDiesDuringTheWait() {
        Flinger flinger = new Flinger();
        Recovery recovery = new Recovery();
        recovery.begin(0);
        LongConsumer sleepThenDie = ms -> {
            flinger.clock[0] += ms;
            recovery.end();
        };
        int status = Recovery.retry(flinger.set(), flinger.allowed(recovery), sleepThenDie);
        assertEquals(DENIED, status);
        assertEquals(1, flinger.calls);
    }
}
