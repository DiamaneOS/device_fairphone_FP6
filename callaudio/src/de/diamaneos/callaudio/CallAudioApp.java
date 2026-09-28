/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The DiamaneOS Project
 */

package de.diamaneos.callaudio;

import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Process;
import android.telephony.TelephonyManager;

import java.util.ArrayList;
import java.util.List;

/**
 * The bridge has no activity, service or receiver in its manifest. It is a persistent,
 * direct-boot-aware system app, so the system starts its process at boot, before the first
 * unlock, and restarts it if it dies. Like the stock QtiTelephonyService it replaces, it keeps
 * one radio-daemon client per active modem (slot 1 to getActiveModemCount()).
 */
public final class CallAudioApp extends Application {
    static final String TAG = "CallAudio";

    private final List<SlotClient> slots = new ArrayList<>(); // Main thread only.
    private AudioServerMonitor audioServer;

    @Override
    public void onCreate() {
        super.onCreate();
        if (!Process.myUserHandle().isSystem()) return;
        audioServer = new AudioServerMonitor();
        audioServer.start();
        TelephonyManager telephony = getSystemService(TelephonyManager.class);
        setSlotCount(telephony.getActiveModemCount());
        // A protected broadcast that the phone process sends; it only reaches exported receivers.
        registerReceiver(
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        setSlotCount(
                                intent.getIntExtra(
                                        TelephonyManager.EXTRA_ACTIVE_SIM_SUPPORTED_COUNT,
                                        telephony.getActiveModemCount()));
                    }
                },
                new IntentFilter(TelephonyManager.ACTION_MULTI_SIM_CONFIG_CHANGED),
                RECEIVER_EXPORTED);
    }

    /** Adds clients from the bottom and disposes them from the top, as stock. */
    private void setSlotCount(int count) {
        while (slots.size() < count) {
            SlotClient slot = new SlotClient(slots.size() + 1, audioServer);
            slots.add(slot);
            slot.start();
        }
        while (slots.size() > count) {
            slots.remove(slots.size() - 1).dispose();
        }
    }
}
