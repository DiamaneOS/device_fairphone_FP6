# Fairphone 6 product configuration

Android product `diamaneos_FP6`, installed at `device/fairphone/FP6` in the
pinned GrapheneOS source workspace. Shared product configuration belongs at
`vendor/diamaneos`.

This is integration source, not a qualified bootable product. Native product
graph, selected source HAL compilation and enforcing USER policy checks have
passed with recorded candidate inputs. Combined installed artifacts and device
behavior still require verification. Required generated inputs are:

- `vendor/fairphone/FP6`: selected stock-derived inputs and reviewed source HAL,
  init and VINTF integration. Device policy is maintained under `sepolicy/`.
- `device/fairphone/FP6-kernel`: matched kernel, module and device-tree artifacts,
  generated product/board definitions and load lists.

Both includes are mandatory. Missing inputs must fail the build. Generated
artifacts are not stored in this repository. The product uses development
signing identities and does not set upstream official-build identity.

Hardware bindings come from Fairphone Gerrit `vendor/fairphone/fps` at
`51648f54c2a7d31fc24833adcae3758cb251966f` and
`platform/vendor/qcom/volcano` at
`0ce43e6912dd237ea76cfd0324266bde8330d07a`. The handwritten configuration is
original integration code using those hardware facts; this repository does
not import their manufacturing product, binary userdata or release keys.
Partition capacities and AVB/layout must also pass image checks before use.

Original code is Apache-2.0; see LICENSE. Framework code remains in its pinned
upstream projects under its existing licences.

`boot/fstab.qcom` retains the published FP6 UFS mount/encryption settings,
including the shipped early `/odm/persist` mount. Its original copyright and
licence remain in the file; see NOTICE for provenance. It is installed in the
vendor ramdisk and vendor image, selected by `androidboot.fstab_suffix=qcom`.
The optional Google GSI public-key paths are omitted: system, system_ext and
product use the declared development vbmeta_system chain. No device-specific
persistent data is included in the source or generated vendor inputs.

This mount configuration still requires native fs_mgr, SELinux, encryption and
image verification before a device boot. Recovery image verification
remains pending. Existing formattable flags for writable device
partitions are preserved from the pinned source; this is not a flashing command.

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
