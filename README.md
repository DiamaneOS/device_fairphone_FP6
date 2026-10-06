# Fairphone 6 device configuration

Android product `FP6` (`lunch FP6-cur-userdebug`), at `device/fairphone/FP6` in
the GrapheneOS workspace. Shared DiamaneOS configuration is in `vendor/diamaneos`.

- Keeps Fairphone's product identity (brand, device, model) in the fingerprint,
  as GrapheneOS keeps Google's; build ID, number and keys are our own.
- Needs two generated inputs, and the build fails without either:
  - `vendor/fairphone/FP6`: selected stock vendor files, HAL, init and VINTF
    integration;
  - `device/fairphone/FP6-kernel`: kernel, modules, device tree, board
    definitions and module load lists.
- Hardware facts come from Fairphone's `vendor/fairphone/fps` (`51648f54`) and
  `platform/vendor/qcom/volcano` (`0ce43e69`). No manufacturing product,
  userdata or keys are imported.
- Release signing is done by diamaneos-tools (`docs/SIGNING.md`).

## Layout

| Path | Contents |
|---|---|
| `boot/` | fstab, init and ueventd files, module load lists, recovery init |
| `power/` | power HAL config, camera hint client, power stats HAL, task profiles |
| `audio/`, `callaudio/` | audio packages, AOSP effects; the call-audio bridge app |
| `bluetooth/` | HCI service with its seccomp filter |
| `camera/`, `media/` | stock CamX camera; hardware video encoders (decoders stay software) |
| `modem/`, `telephony/`, `gnss/`, `nfc/`, `wifi/` | radios and their configuration |
| `fingerprint/` | fingerprint HAL over the stock module |
| `timekeep/` | `timekeepd`: keeps the clock across reboots |
| `compat/` | compatibility libraries for stock vendor files |
| `rro/`, `overlay/` | resource overlays |
| `sepolicy/` | device policy ([sepolicy/README.md](sepolicy/README.md)) |

## Boot and images

- 4 KiB pages. Boot, init_boot and recovery use header v4; recovery has no
  kernel (stock layout). OS version and patch level are in AVB properties.
- Android and recovery share one fstab (`boot/fstab.qcom`, stock settings).
- Vendor modules load after the firmware mounts. `boot/modules/` splits the
  kernel's `modules.load` into a platform list, then per-subsystem lists loaded
  in parallel. A kernel change to `modules.load` needs the same change here, or
  the image checks fail.
- AVB chain as on stock: recovery 1, vbmeta_system 2, boot 3, init_boot 4.
  vbmeta_system covers pvmfw too (the bootloader rejects a slot without it).
  dm-verity uses SHA-256.
- Boot control: Fairphone's `hardware/qcom/bootctrl` and `recovery-ext`, with
  CFI. It runs as `vendor_bootctl` with CAP_SYS_RAWIO only and can open only
  the A/B GPT disks, misc and the UFS BSG node.
- Recovery (`boot/init.recovery.qcom.rc`): USB peripheral mode, modem firmware
  mounted read-only before ADSP boot, platform wipe hooks only. Includes
  fastbootd.

## Services

- Stock FP6 thermal, lights, vibrator, USB and health HALs.
- Power: LineageOS libperfmgr (unmodified) with `power/powerhint.json`, as
  system with CAP_SYS_NICE. Camera hints reach it through
  `libqti-perfd-client`; Qualcomm's perf daemon is not installed.
- Power stats: our own HAL. It reports SoC and subsystem sleep times from
  `/dev/stats`; there are no energy meters (no on-device power monitor).

## Licence

Apache-2.0 ([LICENSE](LICENSE)). Third-party files keep their own licences;
see [NOTICE](NOTICE) and `sepolicy/provenance.json`.
