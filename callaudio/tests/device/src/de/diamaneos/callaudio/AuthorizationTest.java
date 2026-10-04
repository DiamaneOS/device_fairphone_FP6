// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project
package de.diamaneos.callaudio;

import static org.junit.Assert.*;
import android.content.pm.PackageManager;
import android.media.AudioSystem;
import android.system.OsConstants;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Run only with no active call on the paired guarded image. No call/modem operation. */
@RunWith(AndroidJUnit4.class)
public final class AuthorizationTest {
    static { System.loadLibrary("fp6_callaudio_guard_test"); }
    private static native int setParametersNative(String parameters);
    @Test public void ordinaryAudioPermissionDoesNotAuthorizeVoiceSessionKeys() {
        var context = InstrumentationRegistry.getInstrumentation().getContext();
        assertEquals(PackageManager.PERMISSION_GRANTED,
                context.checkSelfPermission("android.permission.MODIFY_AUDIO_SETTINGS"));
        assertEquals(PackageManager.PERMISSION_DENIED,
                context.checkSelfPermission("android.permission.DIAMANEOS_CONTROL_CALL_AUDIO"));
        // Invalid VSID provides no live voice-session target even if this
        // regression fails. The guarded path must refuse before calling a HAL.
        // JNI's public wrapper maps native failures to a generic Java code.
        // Preserve status_t here so HAL EINVAL cannot look like authorization denial.
        assertEquals(-OsConstants.EPERM, setParametersNative("vsid=0;call_state=1"));
    }
    @Test public void ordinaryAppCannotReadCallStateParameters() {
        assertEquals("", AudioSystem.getParameters("all_call_states"));
    }
}
