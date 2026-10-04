// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project
package de.diamaneos.callaudio;

import static org.junit.Assert.*;
import android.os.IBinder;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Test;
import org.junit.runner.RunWith;
import vendor.qti.hardware.radio.am.*;

/** Actual Android worker/production adapter, local fake radio only; no HAL or modem lookup. */
@RunWith(AndroidJUnit4.class)
public final class SlotLifecycleTest {
    private static final class FakeRadio extends IQcRilAudio.Stub {
        final CountDownLatch entered = new CountDownLatch(1);
        final CountDownLatch release = new CountDownLatch(1);
        final CountDownLatch replacement = new CountDownLatch(1);
        final AtomicInteger registrations = new AtomicInteger();
        final AtomicReference<IQcRilAudioRequest> first = new AtomicReference<>();
        final AtomicReference<IQcRilAudioRequest> latest = new AtomicReference<>();
        public IQcRilAudioResponse setRequestInterface(IQcRilAudioRequest request) {
            int count = registrations.incrementAndGet();
            if (count == 1) {
                first.set(request); entered.countDown();
                try { if (!release.await(5, TimeUnit.SECONDS)) throw new AssertionError("fixture release timeout"); }
                catch (InterruptedException error) { throw new AssertionError(error); }
            }
            latest.set(request);
            if (count == 2) replacement.countDown();
            return new IQcRilAudioResponse.Stub() {
                public void setParametersResponse(int token, int error) { throw new AssertionError("No audio request expected"); }
                public void queryParametersResponse(int token, String value) { throw new AssertionError("No audio query expected"); }
                public int getInterfaceVersion() { return IQcRilAudioResponse.VERSION; }
                public String getInterfaceHash() { return IQcRilAudioResponse.HASH; }
            };
        }
        public void setError(int error) { }
        public int getInterfaceVersion() { return IQcRilAudio.VERSION; }
        public String getInterfaceHash() { return IQcRilAudio.HASH; }
    }
    private static SlotClient client(AudioServerMonitor monitor, FakeRadio radio) {
        return new SlotClient(1, monitor, new SlotClient.Services() {
            public boolean declared(String name) { return true; }
            public IBinder lookup(String name) { return radio.asBinder(); }
        });
    }
    private static boolean current(IQcRilAudioRequest request) throws Exception {
        Method method = request.getClass().getDeclaredMethod("current");
        method.setAccessible(true); return (Boolean) method.invoke(request);
    }
    private static void dispose(SlotClient client, FakeRadio radio) throws Exception {
        radio.release.countDown(); client.dispose();
        Field field = SlotClient.class.getDeclaredField("thread");field.setAccessible(true);
        Thread worker = (Thread) field.get(client);worker.join(5_000);assertFalse(worker.isAlive());
    }
    @Test public void blockedOldRegistrationCannotOvertakeReenabledSlot() throws Exception {
        FakeRadio radio = new FakeRadio();
        AudioServerMonitor monitor = new AudioServerMonitor(AudioError.STATUS_OK);
        SlotClient client = client(monitor, radio);
        try {
            client.start();assertTrue(radio.entered.await(5, TimeUnit.SECONDS));
            client.setEnabled(false);client.setEnabled(true);
            assertEquals(1, radio.registrations.get());
            assertFalse(current(radio.first.get()));
            radio.release.countDown();assertTrue(radio.replacement.await(5, TimeUnit.SECONDS));
            assertNotSame(radio.first.get(), radio.latest.get());
            assertTrue(current(radio.latest.get()));
            Field field = AudioServerMonitor.class.getDeclaredField("listeners");field.setAccessible(true);
            assertEquals(1, ((List<?>) field.get(monitor)).size());
        } finally { dispose(client, radio); }
    }
    @Test public void disabledSlotStaysFencedAfterItsPhysicalRegistrationReturns() throws Exception {
        FakeRadio radio = new FakeRadio();
        SlotClient client = client(new AudioServerMonitor(AudioError.STATUS_OK), radio);
        try {
            client.start();assertTrue(radio.entered.await(5, TimeUnit.SECONDS));
            client.setEnabled(false);assertFalse(current(radio.first.get()));
            radio.release.countDown();
            assertFalse(radio.replacement.await(100, TimeUnit.MILLISECONDS));
            assertEquals(1, radio.registrations.get());
        } finally { dispose(client, radio); }
    }
}
