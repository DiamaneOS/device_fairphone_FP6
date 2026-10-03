/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

/** Bounds retained parameter work for one radio registration, including running work. */
final class ParameterBudget {
    static final int MAX_REQUESTS = 64;
    // At most 1 MiB of UTF-16 payload per slot. Count also bounds empty requests.
    static final int MAX_CHARACTERS = 512 * 1024;
    private int requests;
    private int characters;
    private boolean closed;

    synchronized Reservation acquire(int size) {
        if (closed || size < 0 || size > MAX_CHARACTERS - characters
                || requests >= MAX_REQUESTS) return null;
        requests++;
        characters += size;
        return new Reservation(size);
    }

    synchronized void close() {
        closed = true;
        requests = 0;
        characters = 0;
    }

    final class Reservation implements AutoCloseable {
        private final int size;
        private boolean released;

        private Reservation(int size) { this.size = size; }

        @Override public void close() {
            synchronized (ParameterBudget.this) {
                if (released) return;
                released = true;
                if (!closed) {
                    requests--;
                    characters -= size;
                }
            }
        }
    }
}
