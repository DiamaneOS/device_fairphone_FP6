# FP6 SystemUI overlay

SystemUI reads `security_settings_sfps_enroll_find_sensor_message` as the
accessibility description of the side-sensor icon in the biometric prompt. Like
the Settings overlay, it replaces the Pixel hardware description with Settings'
translated right-side message in every locale. The side-sensor indicator
animation itself follows the sensor location reported by the fingerprint HAL
and needs no overlay.

`doze_display_state_supported` lets the always-on display put the panel in
its low-power doze mode instead of keeping it fully on, as stock does
(`res/values/config.xml`).
