# FP6 Settings overlay

- `config_show_smooth_display`: show Display > Smooth display.
- `config_show_hardware_video_decoding`: show Security & privacy > Exploit
  protection > Hardware video decoding (device `media/media.mk`).
- Side fingerprint sensor education. AOSP's side-sensor animations draw a
  tablet with the sensor on its top edge. The files in `res/raw` are derived
  from AOSP Settings' `fingerprint_edu_lottie_portrait_bottom_right.json`
  (Apache-2.0) by `make_fingerprint_edu.py`: the device outline has the FP6's
  proportions and the sensor sits on the right edge, 44% of the way down,
  where the fingerprint HAL reports it (y 1100 of 2484). The choreography,
  colours and layer names (used by Settings' light-theme colour mapping) are
  AOSP's.
- Settings picks the file by display rotation, and uses the `folded_*` names
  whenever the smallest screen width is below 600 dp. The FP6 is 372 dp wide
  (1116 px at 480 dpi), so the `folded_*` files are the ones shown; the other
  four are identical copies in case that ever changes. Display rotation 0
  shows `*_top_right`, 90 `*_top_left`, 180 `*_bottom_left` and 270
  `*_bottom_right`; each has the root rotation (90, 0, 270, 180 degrees) that
  keeps the drawn sensor on the phone's right edge.
- Regenerate after an AOSP update with
  `make_fingerprint_edu.py packages/apps/Settings/res/raw/fingerprint_edu_lottie_portrait_bottom_right.json res/raw`.
- The side-sensor description uses Settings' translated
  `security_settings_enroll_find_sensor_right_side_message` text in every
  locale instead of the Pixel hardware description.
- Security & privacy > Exploit protection > USB-C port > Off: GrapheneOS's
  summary says Off turns off the port. On the FP6 Off turns off USB data and
  charging, while the ADSP firmware keeps running Type-C and USB PD, so the
  summary names those two and keeps GrapheneOS's note on the separate
  charging mode. GrapheneOS ships these strings in English only, so the
  overlay has no translations either.
