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
import android.os.Binder;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;

import vendor.qti.hardware.radio.am.AudioError;
import vendor.qti.hardware.radio.am.IQcRilAudio;
import vendor.qti.hardware.radio.am.IQcRilAudioRequest;
import vendor.qti.hardware.radio.am.IQcRilAudioResponse;

import java.util.NoSuchElementException;

/**
 * One radio-daemon client (IQcRilAudio/slotN). Registration, re-registration after the daemon
 * dies, setError reports, and accepted parameter work run in order on the slot's own thread.
 * The daemon's oneway calls reach the request binder one at a time and are
 * queued here in that order, so a slot's call-state changes reach AudioFlinger in the order the
 * daemon sent them. The parameter strings (vsid, call_state, call_type, crs_call,
 * isCRSsupported) are validated against AudioParameters and never logged; the daemon chooses
 * the vsid. Unknown keys and malformed values never reach AudioFlinger.
 * Pending/running payloads are bounded per registration. Overflow replies use that
 * registration's oneway endpoint without adding Handler work; an overfull registration
 * that has not yet published its endpoint is revoked and retried.
 */
final class SlotClient implements AudioServerMonitor.Listener {
    private static final long RECONNECT_MS = 1000;
    private static final long UNDECLARED_RETRY_MS = 60_000;

    private final int slot;
    private final String instance;
    private final AudioServerMonitor audioServer;
    private final HandlerThread thread;
    private final Handler handler;
    private volatile Request request;
    private volatile boolean disposed;

    // Slot thread only.
    private StatusRelay relay;
    private IBinder binder;
    private IQcRilAudio service;
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
                    if (disposed) return;
                    long[] start = audioServer.addListener(this);
                    if (disposed) { audioServer.removeListener(this); return; }
                    relay = new StatusRelay((int) start[0], start[1]);
                    connect();
                });
    }

    /** The slot is no longer active (single-SIM mode): drop the client, as stock. */
    void dispose() {
        disposed = true;
        Request current = request;
        if (current != null) current.invalidate();
        audioServer.removeListener(this);
        handler.post(this::clear);
        thread.quitSafely();
    }

    @Override
    public void onAudioServerStatus(int status, long sequence) {
        handler.post(() -> {
            if (!disposed && relay != null) send(relay.onAudioStatus(status, sequence));
        });
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
        // Do not block disposal indefinitely while the radio daemon is absent.
        IBinder b = ServiceManager.checkService(instance);
        if (disposed) return;
        if (b == null) {
            handler.postDelayed(this::connect, RECONNECT_MS);
            return;
        }
        Request next = new Request();
        request = next;
        IBinder.DeathRecipient d = () -> {
            next.invalidate(); // Fence retries/queued work immediately on the binder thread.
            handler.post(() -> lost(b, next));
        };
        try {
            b.linkToDeath(d, 0);
        } catch (RemoteException e) {
            next.invalidate();
            if (request == next) request = null;
            // Right after a death servicemanager can still hand out the old binder.
            handler.postDelayed(this::connect, RECONNECT_MS);
            return;
        }
        binder = b;
        death = d;
        service = IQcRilAudio.Stub.asInterface(b);
        try {
            IQcRilAudioResponse returned = service.setRequestInterface(next);
            if (returned == null) throw new RemoteException();
            next.publishResponse(returned);
        } catch (RemoteException | RuntimeException e) {
            Log.w(TAG, "slot " + slot + ": registration failed");
            clear();
            handler.postDelayed(this::connect, RECONNECT_MS);
            return;
        }
        if (!next.current()) {
            clear();
            if (!disposed) handler.postDelayed(this::connect, RECONNECT_MS);
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

    private void lost(IBinder b, Request owner) {
        if (b != binder || owner != request) return;
        clear();
        Log.w(TAG, "slot " + slot + ": radio daemon gone; waiting for it");
        handler.post(this::connect);
    }

    private void clear() {
        Request current = request;
        if (current != null) current.invalidate();
        request = null;
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
        if (relay != null) relay.onUnregistered();
    }

    private void send(int status) {
        Request current = request;
        if (status == StatusRelay.NONE || service == null || current == null
                || !current.current()) return;
        try {
            service.setError(status);
            relay.onSent(status);
        } catch (RemoteException e) {
            // The death notice follows and re-registers.
        }
    }

    /** Stock AudioController.setParameters, plus retries while audioserver recovers. */
    private void setParameters(Request owner, int token, String params) {
        if (!owner.current()) return;
        int error = AudioError.GENERIC_FAILURE;
        if (audioServer.isRunning()) {
            String pairs = params != null ? params : "";
            int status = Recovery.retry(
                    () -> owner.current() ? AudioSystem.setParameters(pairs) : AudioError.GENERIC_FAILURE,
                    () -> owner.current() && audioServer.retryAllowed(),
                    SystemClock::sleep);
            if (status != AudioSystem.SUCCESS) {
                Log.w(TAG, "slot " + slot + ": AudioFlinger did not apply parameters: " + status);
            }
            // Stock reports success whenever audioserver was running and the call returned.
            error = AudioError.STATUS_OK;
        }
        if (!owner.current()) return;
        IQcRilAudioResponse r = owner.response;
        if (r == null) return;
        try {
            r.setParametersResponse(token, error);
        } catch (RemoteException e) {
            // The death notice follows and re-registers.
        }
    }

    /** Stock AudioController.getParameters: an empty string when audioserver is down. */
    private void queryParameters(Request owner, int token, String params) {
        if (!owner.current()) return;
        String result = "";
        if (audioServer.isRunning() && params != null) {
            String value = AudioSystem.getParameters(params);
            if (value != null) result = value;
        }
        if (!owner.current()) return;
        IQcRilAudioResponse r = owner.response;
        if (r == null) return;
        try {
            r.queryParametersResponse(token, result);
        } catch (RemoteException e) {
            // The death notice follows and re-registers.
        }
    }

    /** Called by the radio daemon on binder threads; queued in arrival order. */
    private final class Request extends IQcRilAudioRequest.Stub {
        private volatile boolean valid = true;
        // Overflow replies run on the admitted binder caller's thread. Frozen replies are oneway.
        private volatile IQcRilAudioResponse response; // Never belongs to a successor.
        private final ParameterBudget budget = new ParameterBudget();

        synchronized void invalidate() {
            valid = false;
            // Serialize with posting: a dead registration cannot retain queued strings.
            handler.removeCallbacksAndMessages(this);
            budget.close();
        }
        boolean current() { return valid && !disposed && request == this; }

        synchronized void publishResponse(IQcRilAudioResponse value) {
            // Serialize publication with startup overflow, after registration IPC.
            // Revocation must be visible to connect() before it accepts registration.
            if (current()) response = value;
        }

        private synchronized boolean enqueue(int token, String params, boolean query) {
            if (!current()) return true; // Stale registrations have no response obligation.
            ParameterBudget.Reservation reserved = budget.acquire(params == null ? 0 : params.length());
            if (reserved == null) return false;
            boolean posted = handler.postAtTime(() -> {
                try (reserved) {
                    if (query) SlotClient.this.queryParameters(this, token, params);
                    else SlotClient.this.setParameters(this, token, params);
                }
            }, this, SystemClock.uptimeMillis());
            if (!posted) reserved.close();
            return posted;
        }

        private void rejected(int token, boolean query) {
            final IQcRilAudioResponse r;
            synchronized (this) {
                if (!current()) return;
                r = response;
                if (r == null) {
                    // A flooding registration has not returned its reply endpoint yet.
                    // Revoke it rather than retain unbounded failed tokens or pretend
                    // its startup succeeded. connect() retries after that call returns.
                    invalidate();
                    Log.w(TAG, "slot " + slot + ": registration exceeded pending audio budget");
                    return;
                }
            }
            try {
                if (query) r.queryParametersResponse(token, "");
                else r.setParametersResponse(token, AudioError.GENERIC_FAILURE);
            } catch (RemoteException ignored) {
                // Registration death handles reconnection; no payload is logged.
            }
        }

        @Override
        public void setParameters(int token, String params) {
            if (Binder.getCallingUid() != Process.PHONE_UID || !current()) return;
            if (!AudioParameters.validSet(params)) { rejected(token, false); return; }
            if (!enqueue(token, params, false)) rejected(token, false);
        }

        @Override
        public void queryParameters(int token, String params) {
            if (Binder.getCallingUid() != Process.PHONE_UID || !current()) return;
            if (!AudioParameters.validQuery(params)) { rejected(token, true); return; }
            if (!enqueue(token, params, true)) rejected(token, true);
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
