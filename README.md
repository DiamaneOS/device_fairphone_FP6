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
