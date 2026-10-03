/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

import static org.junit.Assert.*;
import org.junit.Test;

public final class ParameterBudgetTest {
    @Test public void emptyRequestsCannotBypassCountBound() {
        ParameterBudget budget = new ParameterBudget();
        ParameterBudget.Reservation[] held = new ParameterBudget.Reservation[ParameterBudget.MAX_REQUESTS];
        for (int i = 0; i < held.length; i++) {
            held[i] = budget.acquire(0);
            assertNotNull(held[i]);
        }
        assertNull(budget.acquire(0));
        held[0].close();
        held[0].close(); // Repeated completion cannot create a second free slot.
        assertNotNull(budget.acquire(0));
        assertNull(budget.acquire(0));
    }

    @Test public void runningPayloadRetainsItsReservationUntilCompletion() {
        ParameterBudget budget = new ParameterBudget();
        ParameterBudget.Reservation running = budget.acquire(ParameterBudget.MAX_CHARACTERS);
        assertNotNull(running);
        assertNull(budget.acquire(1));
        assertNull(budget.acquire(Integer.MAX_VALUE));
        assertNull(budget.acquire(-1));
        running.close(); // Used on normal completion, exceptions and failed Handler posts.
        assertNotNull(budget.acquire(ParameterBudget.MAX_CHARACTERS));
    }

    @Test public void revokedRegistrationCannotAffectReplacementBudget() {
        ParameterBudget old = new ParameterBudget();
        ParameterBudget.Reservation stale = old.acquire(ParameterBudget.MAX_CHARACTERS);
        old.close();
        assertNull(old.acquire(0));
        ParameterBudget next = new ParameterBudget();
        assertNotNull(next.acquire(ParameterBudget.MAX_CHARACTERS));
        stale.close();
        stale.close();
        assertNull(next.acquire(1));
        assertNull(old.acquire(0));
    }
}
