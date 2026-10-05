# Fairphone 6 device configuration

Android product `FP6` (`lunch FP6-cur-user`), at `device/fairphone/FP6` in the
GrapheneOS workspace. Shared DiamaneOS configuration is in `vendor/diamaneos`.

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

The product uses the published FP6 thermal, lights, vibrator, USB and health
services. Power is LineageOS's libperfmgr power HAL, pinned unmodified, with the
FP6 configuration in `power/`: the hint table (`powerhint.json`), its init file
(the HAL runs as system with CAP_SYS_NICE only) and a no-op
`libqti-perfd-client` for the stock camera; Qualcomm's closed perf2 daemon is
not installed. The implementations remain separate from the device
configuration. Required runtime dependencies, firmware and policy belong to the
generated integration and must be checked before boot acceptance.

The kernel and vendor ELF contract uses 4 KiB pages. Alignment checking remains
enabled. Boot, init_boot and recovery use header version 4; recovery excludes the
kernel, matching the stock bootloader layout. Android and recovery use the same
fstab so encryption definitions cannot drift. Vendor drivers are loaded after
their firmware mounts, in parallel streams: `boot/modules/` splits the kernel's
vendor_dlkm `modules.load` into a platform list, loaded first, and one list per
subsystem, loaded together after it (`boot/init.qcom.rc`). Each list keeps the
kernel list's order, and together they name every module once; a kernel update
that changes `modules.load` needs the same change here, or image verification
fails. Image-header, policy and native product checks remain mandatory before
flashing.

Recovery has its own `boot/init.recovery.qcom.rc`, imported by the platform
recovery as `init.recovery.qcom.rc`. It selects configfs and the FP6 controller,
sets peripheral mode and mounts the current-slot modem firmware read-only before
requesting ADSP boot. The platform owns ADB/sideload/fastboot compositions;
normal Android's zygote-triggered gadget setup is not used in recovery. The
recovery UI uses the panel backlight/max-brightness nodes and RGBX layout.
First-stage init owns the generated `modules.load.recovery` list. Recovery uses
the default platform wipe hooks; it adds no device-specific erase operation.
Native packaging/policy checks and an actual recovery boot are separate gates.

The minimal product explicitly selects the platform recovery runtime group and
fastbootd. Enabling recovery image generation does not select those packages by
itself. Image verification must check the recovery/init executables, main init
script, USB properties and runtime dependencies as well as the image header.

The generic ramdisk is selected separately through the platform's
`generic_ramdisk.mk`. Verify the actual `init_boot` ramdisk contains an executable,
statically linked ARM64 `/init`, `snapuserd_ramdisk` and its build properties;
an image with a valid header and AVB hash can still lack its runtime. The GKI v4
boot, init_boot and recovery headers carry zero OS-version fields. Version and
patch information remains in AVB properties. Boot's patch level follows the
selected vendor/kernel baseline; init_boot follows the platform.

AVB uses the published FP6 chain locations: recovery 1, vbmeta_system 2,
boot 3 and init_boot 4. Verify all four signatures and child descriptors against
the selected development key, with verification flags zero. Matching this
layout and passing host checks do not establish that the device boots.
