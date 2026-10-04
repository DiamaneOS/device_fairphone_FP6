/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */
package de.diamaneos.callaudio;
import static org.junit.Assert.*;
import org.junit.Test;

public final class AudioParametersTest {
    @Test public void stockCallControlAndQueriesRemainAccepted() {
        assertTrue(AudioParameters.validSet("vsid=281022464;call_state=2;call_type=1;crs_call=false"));
        assertTrue(AudioParameters.validSet("vsid=4294967295;call_state=1;crs_call=true"));
        assertTrue(AudioParameters.validSet("vsid=-1;call_state=1"));
        assertTrue(AudioParameters.validQuery("isCRSsupported=1"));
        assertTrue(AudioParameters.validQuery("all_call_states"));
    }
    @Test public void unrelatedKeysAndParserAmbiguitiesNeverReachTheHal() {
        for (String value : new String[] { null, "", "vsid=1", "call_state=2",
                "vsid=1;call_state=2;routing=8", "vsid=1;vsid=2;call_state=2",
                "vsid=1;call_state=2;", "vsid=1;call_state=2=3", "vsid=1;call_state= 2",
                "vsid=1;call_state=２", "vsid=4294967296;call_state=2",
                "vsid=1;call_state=2147483648", "vsid=1;call_state=2;crs_call=1",
                "vsid=1;call_state=2\u0000", "vsid=" + "1".repeat(300) + ";call_state=2" }) {
            assertFalse(AudioParameters.validSet(value));
        }
        for (String value : new String[] { null, "", "routing", "all_call_states;vendor_secret",
                "isCRSsupported=1;vsid=1", "isCRSsupported=2" }) {
            assertFalse(AudioParameters.validQuery(value));
        }
    }
}
