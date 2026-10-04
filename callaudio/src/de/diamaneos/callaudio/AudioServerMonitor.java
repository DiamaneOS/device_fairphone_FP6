/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

import static de.diamaneos.callaudio.CallAudioApp.TAG;

import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.util.Log;

import vendor.qti.hardware.radio.am.AudioError;

import java.util.ArrayList;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.concurrent.CountDownLatch;

/**
 * Whether audioserver is running, learned without any permission from a death link on
 * media.audio_flinger. (Stock uses AudioManager.setAudioServerStateCallback, which needs
 * MODIFY_AUDIO_ROUTING or MODIFY_PHONE_STATE.) The status is an AudioError value:
 *
 * <ul>
 *   <li>STATUS_OK once media.audio_flinger and media.audio_policy are both registered.
 *       audioserver registers them after its HAL modules are loaded.
 *   <li>STATUS_SERVER_DIED from the death notice until then.
 *   <li>GENERIC_FAILURE if audioserver was not running when the bridge started (stock's
 *       initial "not running" report).
 * </ul>
 */
final class AudioServerMonitor {
    interface Listener {
        /** Called on the monitor thread, in order, with the monitor's lock held: post, don't block. */
        void onAudioServerStatus(int status, long sequence);
    }

    private static final String AUDIO_FLINGER = "media.audio_flinger";
    private static final String AUDIO_POLICY = "media.audio_policy";
    private static final long RETRY_MS = 500;

    private final Object lock = new Object();
    private final List<Listener> listeners = new ArrayList<>(); // Guarded by lock.
    private final Recovery recovery = new Recovery();
    private int status; // Guarded by lock.
    private long sequence; // Guarded by lock.

    AudioServerMonitor() {
        this(ServiceManager.checkService(AUDIO_FLINGER) != null
                ? AudioError.STATUS_OK
                : AudioError.GENERIC_FAILURE);
    }

    AudioServerMonitor(int initialStatus) { status = initialStatus; }

    void start() {
        Thread thread = new Thread(this::watch, "CallAudioServer");
        thread.setDaemon(true);
        thread.start();
    }

    /** Returns the {status, sequence} the listener starts from; later changes come in order. */
    long[] addListener(Listener listener) {
        synchronized (lock) {
            listeners.add(listener);
            return new long[] {status, sequence};
        }
    }

    /** The current {status, sequence}; changes after it reach the listeners after this returns. */
    long[] snapshot() {
        synchronized (lock) {
            return new long[] {status, sequence};
        }
    }

    void removeListener(Listener listener) {
        synchronized (lock) {
            listeners.remove(listener);
        }
    }

    boolean isRunning() {
        synchronized (lock) {
            return status == AudioError.STATUS_OK;
        }
    }

    /** Whether a rejected setParameters should be tried again (see {@link Recovery}). */
    boolean retryAllowed() {
        return isRunning() && recovery.active(SystemClock.elapsedRealtime());
    }

    private void publish(int newStatus) {
        synchronized (lock) {
            if (newStatus == status) return;
            status = newStatus;
            sequence++;
            for (Listener listener : listeners) listener.onAudioServerStatus(status, sequence);
        }
    }

    private void watch() {
        boolean restarted;
        synchronized (lock) {
            restarted = status != AudioError.STATUS_OK;
        }
        while (true) {
            IBinder flinger = ServiceManager.waitForService(AUDIO_FLINGER);
            IBinder policy = flinger != null ? ServiceManager.waitForService(AUDIO_POLICY) : null;
            CountDownLatch died = new CountDownLatch(1);
            IBinder.DeathRecipient recipient = died::countDown;
            if (!link(flinger, recipient)) {
                SystemClock.sleep(RETRY_MS);
                continue;
            }
            // Right after a death servicemanager can still hand out the old binders.
            if (policy == null || !flinger.pingBinder() || !policy.pingBinder()) {
                unlink(flinger, recipient);
                SystemClock.sleep(RETRY_MS);
                continue;
            }
            if (restarted) recovery.begin(SystemClock.elapsedRealtime());
            Log.i(TAG, restarted ? "audioserver is back" : "audioserver is running");
            publish(AudioError.STATUS_OK);
            awaitUninterruptibly(died);
            recovery.end();
            Log.w(TAG, "audioserver died");
            publish(AudioError.STATUS_SERVER_DIED);
            restarted = true;
        }
    }

    private static boolean link(IBinder binder, IBinder.DeathRecipient recipient) {
        if (binder == null) return false;
        try {
            binder.linkToDeath(recipient, 0);
            return true;
        } catch (RemoteException e) {
            return false;
        }
    }

    private static void unlink(IBinder binder, IBinder.DeathRecipient recipient) {
        try {
            binder.unlinkToDeath(recipient, 0);
        } catch (NoSuchElementException e) {
            // Already dead.
        }
    }

    private static void awaitUninterruptibly(CountDownLatch latch) {
        while (true) {
            try {
                latch.await();
                return;
            } catch (InterruptedException e) {
                // Only the death notice ends the wait.
            }
        }
    }
}
