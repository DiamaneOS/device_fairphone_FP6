# FP6 SystemUI overlay

- `security_settings_sfps_enroll_find_sensor_message`: the accessibility
  description of the side-sensor icon in the biometric prompt.
  - Like the Settings overlay, it replaces the Pixel hardware description with
    Settings' translated right-side message in every locale.
  - The side-sensor indicator animation follows the sensor location the
    fingerprint HAL reports and needs no overlay.
- `physical_power_button_center_screen_location_y` and
  `physical_fingerprint_sensor_center_screen_location_*`
  (`res/values/dimens.xml`): the power button with the sensor, on the right
  edge.
  - They place the power-button wake reveal and the fingerprint unlock ripple
    there.
- `doze_display_state_supported`: the always-on display puts the panel in its
  low-power doze mode instead of keeping it fully on, as stock does
  (`res/values/config.xml`).
- `config_volumeDialogOnLeft`: puts the volume dialog on the left edge, next
  to the FP6 volume buttons, as stock FP6 SystemUI does.
  - The resource comes from a DiamaneOS SystemUI change; upstream Android 17
    always shows the dialog on the right.
- `tally_lens_center_x`, `tally_lens_center_y`, `tally_lens_radius`: the front
  camera's lens position and radius, for SystemUI's `tally_lens.xml`.
- `doze_proximity_check_before_tap`: Tap to wake and Tap to check phone check
  the proximity sensor before a double or single tap wakes the screen, and
  drop taps while it is covered.
  - The touch controller reports taps through a pocket too.
  - The resource comes from a DiamaneOS SystemUI change.
