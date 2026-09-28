/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

import static de.diamaneos.callaudio.CallAudioApp.TAG;

import android.media.AudioSystem;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.util.Log;

import vendor.qti.hardware.radio.am.AudioError;
import vendor.qti.hardware.radio.am.IQcRilAudio;
import vendor.qti.hardware.radio.am.IQcRilAudioRequest;
import vendor.qti.hardware.radio.am.IQcRilAudioResponse;

import java.util.NoSuchElementException;

/**
 * One radio-daemon client (IQcRilAudio/slotN). Everything for the slot runs in order on its own
 * thread: registration, re-registration after the daemon dies, setError reports, and the
 * daemon's requests. The daemon's oneway calls reach the request binder one at a time and are
 * queued here in that order, so a slot's call-state changes reach AudioFlinger in the order the
 * daemon sent them. The parameter strings (vsid, call_state, call_type, crs_call,
 * isCRSsupported) are passed through unchanged and never logged; the daemon chooses the vsid.
 */
final class SlotClient implements AudioServerMonitor.Listener {
    private static final long RECONNECT_MS = 1000;
    private static final long UNDECLARED_RETRY_MS = 60_000;

    private final int slot;
    private final String instance;
    private final AudioServerMonitor audioServer;
    private final HandlerThread thread;
    private final Handler handler;
    private final Request request = new Request();
    private volatile boolean disposed;

    // Slot thread only.
    private StatusRelay relay;
    private IBinder binder;
    private IQcRilAudio service;
    private IQcRilAudioResponse response;
    private IBinder.DeathRecipient death;

    SlotClient(int slot, AudioServerMonitor audioServer) {
        this.slot = slot;
        instance = IQcRilAudio.DESCRIPTOR + "/slot" + slot;
        this.audioServer = audioServer;
        thread = new HandlerThread("CallAudioSlot" + slot);
        thread.start();
        handler = new Handler(thread.getLooper());
    }

    void start() {
        handler.post(
                () -> {
                    long[] start = audioServer.addListener(this);
                    relay = new StatusRelay((int) start[0], start[1]);
                    connect();
                });
    }

    /** The slot is no longer active (single-SIM mode): drop the client, as stock. */
    void dispose() {
        disposed = true;
        audioServer.removeListener(this);
        handler.post(this::clear);
        thread.quitSafely();
    }

    @Override
    public void onAudioServerStatus(int status, long sequence) {
        handler.post(() -> send(relay.onAudioStatus(status, sequence)));
    }

    private void connect() {
        if (disposed) return;
        if (!ServiceManager.isDeclared(instance)) {
            // Not declared, or this domain may not find it (isDeclared reports a denial as
            // false). Neither heals by itself, but the error repeats so that a log taken at the
            // time of a silent call shows it.
            Log.e(TAG, "slot " + slot + ": call-audio service not declared or not allowed;"
                    + " calls will have no audio");
            handler.postDelayed(this::connect, UNDECLARED_RETRY_MS);
            return;
        }
        // Blocks this slot's thread until the radio daemon has registered the service.
        IBinder b = ServiceManager.waitForDeclaredService(instance);
        if (disposed) return;
        IBinder.DeathRecipient d = () -> handler.post(() -> lost(b));
        try {
            if (b == null) throw new RemoteException();
            b.linkToDeath(d, 0);
        } catch (RemoteException e) {
            // Right after a death servicemanager can still hand out the old binder.
            handler.postDelayed(this::connect, RECONNECT_MS);
            return;
        }
        binder = b;
        death = d;
        service = IQcRilAudio.Stub.asInterface(b);
        try {
            response = service.setRequestInterface(request);
        } catch (RemoteException | RuntimeException e) {
            Log.w(TAG, "slot " + slot + ": registration failed");
            clear();
            handler.postDelayed(this::connect, RECONNECT_MS);
            return;
        }
        Log.i(TAG, "slot " + slot + ": registered with the radio daemon");
        // Status changes that arrived while this thread was blocked above are still queued behind
        // it. Take the current status first (nothing is sent while unregistered); the queued
        // updates are then older by sequence and dropped.
        long[] now = audioServer.snapshot();
        relay.onAudioStatus((int) now[0], now[1]);
        send(relay.onRegistered());
    }

    private void lost(IBinder b) {
        if (b != binder) return; // A stale death notice must not clear its successor.
        clear();
        Log.w(TAG, "slot " + slot + ": radio daemon gone; waiting for it");
        // waitForDeclaredService blocks until the restarted daemon registers again.
        handler.post(this::connect);
    }

    private void clear() {
        if (binder != null) {
            try {
                binder.unlinkToDeath(death, 0);
            } catch (NoSuchElementException e) {
                // Already dead.
            }
        }
        binder = null;
        death = null;
        service = null;
        response = null;
        if (relay != null) relay.onUnregistered();
    }

    private void send(int status) {
        if (status == StatusRelay.NONE || service == null) return;
        try {
            service.setError(status);
            relay.onSent(status);
        } catch (RemoteException e) {
            // The death notice follows and re-registers.
        }
    }

    /** Stock AudioController.setParameters, plus retries while audioserver recovers. */
    private void setParameters(int token, String params) {
        int error = AudioError.GENERIC_FAILURE;
        if (audioServer.isRunning()) {
            String pairs = params != null ? params : "";
            int status = Recovery.retry(
                    () -> AudioSystem.setParameters(pairs),
                    audioServer::retryAllowed,
                    SystemClock::sleep);
            if (status != AudioSystem.SUCCESS) {
                Log.w(TAG, "slot " + slot + ": AudioFlinger did not apply parameters: " + status);
            }
            // Stock reports success whenever audioserver was running and the call returned.
            error = AudioError.STATUS_OK;
        }
        IQcRilAudioResponse r = response;
        if (r == null) return;
        try {
            r.setParametersResponse(token, error);
        } catch (RemoteException e) {
            // The death notice follows and re-registers.
        }
    }

    /** Stock AudioController.getParameters: an empty string when audioserver is down. */
    private void queryParameters(int token, String params) {
        String result = "";
        if (audioServer.isRunning() && params != null) {
            String value = AudioSystem.getParameters(params);
            if (value != null) result = value;
        }
        IQcRilAudioResponse r = response;
        if (r == null) return;
        try {
            r.queryParametersResponse(token, result);
        } catch (RemoteException e) {
            // The death notice follows and re-registers.
        }
    }

    /** Called by the radio daemon on binder threads; queued in arrival order. */
    private final class Request extends IQcRilAudioRequest.Stub {
        @Override
        public void setParameters(int token, String params) {
            handler.post(() -> SlotClient.this.setParameters(token, params));
        }

        @Override
        public void queryParameters(int token, String params) {
            handler.post(() -> SlotClient.this.queryParameters(token, params));
        }

        // The daemon can ask; the answers are the vendor's frozen version 1 (Android.bp).
        @Override
        public int getInterfaceVersion() {
            return IQcRilAudioRequest.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return IQcRilAudioRequest.HASH;
        }
    }
}
