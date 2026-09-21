# Fairphone 6 device configuration

Android product `diamaneos_FP6`, installed at `device/fairphone/FP6` in the
pinned GrapheneOS source workspace. Shared product configuration belongs at
`vendor/diamaneos`.

This is initial integration source, not a qualified bootable product. The
Android graph has not yet been resolved. Required generated inputs are:

- `vendor/fairphone/FP6`: selected stock-derived inputs and reviewed source HAL,
  init, VINTF and policy integration.
- `device/fairphone/FP6-kernel`: matched kernel, module and device-tree artifacts,
  generated product/board definitions and load lists.

| Path | Contents |
|---|---|
| `boot/` | fstab, init and ueventd files, module load lists, recovery init |
| `power/` | power HAL config, camera hint client, power stats HAL, task profiles |
| `audio/`, `callaudio/` | audio packages, AOSP effects, the call-audio bridge app ([callaudio/SECURITY.md](callaudio/SECURITY.md)) |
| `bluetooth/` | HCI service with its seccomp filter |
| `camera/`, `media/` | stock CamX camera; hardware video encoders (decoders stay software) |
| `modem/`, `telephony/`, `gnss/`, `nfc/`, `wifi/` | radios and their configuration ([telephony/IMS.md](telephony/IMS.md), [wifi/README.md](wifi/README.md)) |
| `fingerprint/` | fingerprint HAL over the stock module |
| `timekeep/` | `timekeepd`: keeps the clock across reboots |
| `compat/` | compatibility libraries for stock vendor files |
| `rro/` | resource overlays |
| `sepolicy/` | device policy ([sepolicy/README.md](sepolicy/README.md)) |

## Boot and images

- Pages are 4 KiB.
- Boot, init_boot and recovery use header v4. Recovery has no kernel (stock
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

## Services

This mount configuration still requires native fs_mgr, SELinux, encryption and
image verification before a device boot. Recovery fstab and image integration
remain separate pending work. Existing formattable flags for writable device
partitions are preserved from the pinned source; this is not a flashing command.
