# Fairphone 6 device configuration

Android product `FP6` (`lunch FP6-cur-user`), at `device/fairphone/FP6` in the
GrapheneOS workspace. Shared DiamaneOS configuration is in `vendor/diamaneos`.

- Keeps Fairphone's product identity (brand, device, model) in the fingerprint,
  as GrapheneOS keeps Google's; build ID, number and keys are DiamaneOS's.
- Needs two generated inputs; the build fails without either:
  - `vendor/fairphone/FP6`: selected stock vendor files, HAL, init and VINTF
    integration;
  - `device/fairphone/FP6-kernel`: kernel, modules, device tree, board
    definitions and module load lists.
- Hardware facts come from Fairphone's published `vendor/fairphone/fps` and
  `platform/vendor/qcom/volcano` sources; `NOTICE` lists what each file
  derives from.
- No manufacturing product, userdata or keys are imported.
- Release signing is done by diamaneos-tools (`docs/SIGNING.md`).

## Layout

| Path | Contents |
|---|---|
| `boot/` | fstab, init and ueventd files, module load lists, recovery init |
| `power/` | power HAL config, camera hint client, power stats HAL, task profiles |
| `audio/`, `callaudio/` | audio packages, AOSP effects; the call-audio bridge app ([callaudio/SECURITY.md](callaudio/SECURITY.md)) |
| `bluetooth/` | HCI service with its seccomp filter |
| `camera/`, `media/` | stock CamX camera; hardware video encoders; hardware decoders as a user opt-in (off by default) |
| `modem/`, `telephony/`, `gnss/`, `nfc/`, `wifi/` | radios and their configuration ([telephony/IMS.md](telephony/IMS.md), [wifi/README.md](wifi/README.md)) |
| `fingerprint/` | fingerprint HAL over the stock module |
| `timekeep/` | `timekeepd`: keeps the clock across reboots |
| `firmware/` | `fwrelease`: finds the installed Fairphone firmware release for Settings |
| `compat/` | compatibility libraries for stock vendor files |
| `display/` | display configuration: brightness table and high-brightness limits |
| `rro/` | resource overlays |
| `sepolicy/` | device policy ([sepolicy/README.md](sepolicy/README.md)) |

## Boot and images

- Pages are 4 KiB.
- Boot, init_boot and recovery use header v4; recovery has no kernel (stock
  layout). OS version and patch level are in AVB properties.
- Android and recovery share one fstab (`boot/fstab.qcom`, stock settings).
- Vendor modules load after the firmware mounts.
  - `boot/modules/` splits the kernel's `modules.load` into a platform list,
    then per-subsystem lists loaded in parallel.
  - A kernel change to `modules.load` needs the same change here, or the image
    checks fail.
- AVB chain as on stock; rollback index locations: recovery 1, vbmeta_system 2,
  boot 3, init_boot 4.
  - vbmeta_system covers pvmfw too: the bootloader rejects a slot without it.
  - dm-verity uses SHA-256.
- Boot control: Fairphone's `hardware/qcom/bootctrl` and `recovery-ext`, with
  CFI.
  - It runs as `vendor_bootctl` with CAP_SYS_RAWIO only.
  - It can open only the A/B GPT disks, misc and the UFS BSG node
    (`boot/ueventd.rc`).
- Recovery (`boot/init.recovery.qcom.rc`): USB peripheral mode, modem firmware
  mounted read-only before ADSP boot, platform wipe hooks only. Includes
  fastbootd.

## Display

- Brightness comes from `display/display_port_130.xml`: a table measured on the
  panel (white, 20 % window), 36 points from panel level 10 (2.0 nits) to 3480
  (1151 nits).
- Brightness is linear in nits, so equal slider steps look about equal.
  - Android allows a shaped brightness-to-backlight map only through
    `evenDimmer`. While it is on, Android offers no Extra dim switch.
- The manual slider ends at level 2048 (757 nits). Above it only white gets
  brighter: mid greys stay put and colours oversaturate.
- The band above is for sunlight: automatic brightness at 10000 lux or more,
  without a time limit, also in battery saver. The curve reaches the top at
  20000 lux.
- Levels 3481 to 4094 (up to 1364 nits) are not used.
- Range, default and dim values are in
  `rro/FP6FrameworksOverlay/res/values/brightness.xml`. Its minimum must equal
  the table's first backlight point; the image checks compare the table with
  the measured panel curve.

## Services

- Thermal, lights, vibrator, USB and health: the Qualcomm HALs listed in
  `device.mk`, built from source.
- Power: LineageOS libperfmgr (unmodified) with `power/powerhint.json`, as
  system with CAP_SYS_NICE.
  - Camera hints reach it through `libqti-perfd-client`.
  - Qualcomm's perf daemon is not installed.
- Power stats: DiamaneOS's own HAL.
  - It reports SoC and subsystem sleep times from `/dev/stats`.
  - There are no energy meters: the phone has no on-device power monitor.

## Licence

Apache-2.0 ([LICENSE](LICENSE)). Third-party notices: [NOTICE](NOTICE) and
the `provenance.json` files.
