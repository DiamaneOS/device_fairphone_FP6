# Fairphone 6 product configuration

Android product `diamaneos_FP6`, installed at `device/fairphone/FP6` in the
pinned GrapheneOS source workspace. Shared product configuration belongs at
`vendor/diamaneos`.

This is initial integration source, not a qualified bootable product. The
Android graph has not yet been resolved. Required generated inputs are:

- `vendor/fairphone/FP6`: selected stock-derived inputs and reviewed source HAL,
  init, VINTF and policy integration.
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
image verification before a device boot. Recovery fstab and image integration
remain separate pending work. Existing formattable flags for writable device
partitions are preserved from the pinned source; this is not a flashing command.

Boot-control services come from pinned Fairphone `hardware/qcom/bootctrl` and
`vendor/qcom/opensource/recovery-ext` projects. The FP6 UFS BSG configuration
is selected explicitly. Their original notices remain in those projects.
The recovery extension's kernel-header/ION dependencies and upstream CFI
exception need native integration review before the boot-control path is
accepted; selecting these modules does not establish safe slot switching.
