# FP6 Settings overlay

- `config_show_smooth_display`: show Display > Smooth display.
- Side fingerprint sensor education. AOSP draws the side-sensor animations for a
  device whose sensor is on the top edge in its natural (landscape) rotation.
  The FP6 sensor is in the power button on the right edge of the portrait
  panel, a quarter turn away, so each rotation shows AOSP's animation for the
  rotation one step earlier: rotation 0 uses the `portrait_bottom_right` drawing,
  90 `landscape_top_right`, 180 `portrait_top_left` and 270
  `landscape_bottom_left`. The files in `res/raw` are unmodified copies of
  those AOSP Settings animations (Apache-2.0) under the names Settings loads.
- The side-sensor description uses Settings' translated
  `security_settings_enroll_find_sensor_right_side_message` text in every
  locale instead of the Pixel hardware description.
