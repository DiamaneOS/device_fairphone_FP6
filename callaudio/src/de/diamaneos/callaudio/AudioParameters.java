/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */
package de.diamaneos.callaudio;

import java.util.HashSet;
import java.util.Set;

/** Narrow QCRIL call-control contract; never forward unrelated HAL parameters. */
final class AudioParameters {
    // Containment limit, comfortably above four stock keys and 32-bit values.
    static final int MAX_LENGTH = 256;
    private static final int MAX_CALL_TYPE_LENGTH = 32; // Downstream opaque-token containment budget.
    private AudioParameters() {}

    static boolean validSet(String parameters) {
        if (parameters == null || parameters.isEmpty() || parameters.length() > MAX_LENGTH) {
            return false;
        }
        Set<String> keys = new HashSet<>();
        for (String pair : parameters.split(";", -1)) {
            int separator = pair.indexOf('=');
            if (separator <= 0 || separator != pair.lastIndexOf('=')) return false;
            String key = pair.substring(0, separator);
            String value = pair.substring(separator + 1);
            if (!keys.add(key)) return false;
            switch (key) {
                case "vsid":
                    // Vendor str_parms reads an int and then uses its uint32 bit pattern.
                    if (!decimal(value, Integer.MIN_VALUE, 0xffff_ffffL)) return false;
                    break;
                case "call_state":
                    if (!decimal(value, Integer.MIN_VALUE, Integer.MAX_VALUE)) return false;
                    break;
                case "call_type":
                    // The pinned producer sends text, including UNKNOWN. Keep
                    // this opaque HAL value bounded without inventing an enum.
                    if (!callType(value)) return false;
                    break;
                case "crs_call":
                    if (!value.equals("true") && !value.equals("false")) return false;
                    break;
                default:
                    return false;
            }
        }
        return keys.contains("vsid") && keys.contains("call_state");
    }

    private static boolean callType(String value) {
        if (value.isEmpty() || value.length() > MAX_CALL_TYPE_LENGTH) return false;
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                    || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
        }
        return true;
    }

    static boolean validQuery(String parameters) {
        // Exact request strings present in the pinned QCRIL audio implementation.
        return "isCRSsupported=1".equals(parameters)
                || "isCRSsupported".equals(parameters)
                || "all_call_states".equals(parameters);
    }

    private static boolean decimal(String value, long minimum, long maximum) {
        // Longest signed 32-bit decimal has eleven ASCII characters.
        if (value.isEmpty() || value.length() > 11) return false;
        int start = value.charAt(0) == '-' ? 1 : 0;
        if (start == value.length()) return false;
        for (int i = start; i < value.length(); i++) {
            char digit = value.charAt(i);
            if (digit < '0' || digit > '9') return false;
        }
        try {
            long number = Long.parseLong(value);
            return number >= minimum && number <= maximum;
        } catch (NumberFormatException e) {
            return false;
        }
    }
}
