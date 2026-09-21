# Fairphone 6 device configuration

Android product `diamaneos_FP6`, installed at `device/fairphone/FP6` in the
pinned GrapheneOS source workspace. Shared product configuration belongs at
`vendor/diamaneos`.

- Keeps Fairphone's product identity (brand, device, model) in the fingerprint,
  as GrapheneOS keeps Google's. Build ID, number and keys are DiamaneOS's.
- Needs two generated inputs, and the build fails without either:
  - `vendor/fairphone/FP6`: selected stock vendor files, HAL, init and VINTF
    integration
  - `device/fairphone/FP6-kernel`: kernel, modules, device tree, board
    definitions and module load lists
- Hardware facts come from Fairphone's published `vendor/fairphone/fps` and
  `platform/vendor/qcom/volcano` sources. `NOTICE` lists what each file
  derives from.
- No manufacturing product, userdata or keys are imported.
- Release signing is done by diamaneos-tools (`docs/SIGNING.md`).

## Layout

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

- Thermal, lights, vibrator, USB and health: the Qualcomm HALs listed in
  `device.mk`, built from source.
- Power: LineageOS libperfmgr (unmodified) with `power/powerhint.json`, as
  system with CAP_SYS_NICE.
  - Camera hints reach it through `libqti-perfd-client`.
  - Qualcomm's perf daemon is not installed.
- Power stats: DiamaneOS's own HAL.
  - It reports SoC and subsystem sleep times from `/dev/stats`.
  - There are no energy meters: the phone has no on-device power monitor.

Boot-control services come from pinned Fairphone `hardware/qcom/bootctrl` and
`vendor/qcom/opensource/recovery-ext` projects. The FP6 UFS BSG configuration
is selected explicitly. Their original notices remain in those projects.
The normal and recovery implementations compile with CFI enabled. Their UFS
header layouts match the pinned kernel interfaces. Runtime slot switching still
requires device verification.

The product uses the published FP6 power, thermal, lights, vibrator, USB and
health services. Power has a small Soong build adaptation; the implementations
remain separate from the device configuration. Required runtime dependencies,
firmware and policy belong to the generated integration and must be checked
before boot acceptance.

The kernel and vendor ELF contract uses 4 KiB pages. Alignment checking remains
enabled. Boot, init_boot and recovery use header version 4; recovery excludes the
kernel, matching the stock bootloader layout. Android and recovery use the same
fstab so encryption definitions cannot drift. Vendor drivers are loaded after
their firmware mounts. Image-header, policy and native product checks remain
mandatory before flashing.
