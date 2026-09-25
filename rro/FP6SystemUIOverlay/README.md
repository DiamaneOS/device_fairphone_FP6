# FP6 SystemUI overlay

SystemUI reads `security_settings_sfps_enroll_find_sensor_message` as the
accessibility description of the side-sensor icon in the biometric prompt. Like
the Settings overlay, it replaces the Pixel hardware description with Settings'
translated right-side message in every locale. The side-sensor indicator
animation itself follows the sensor location reported by the fingerprint HAL
and needs no overlay.
