# FP6 service policy

Policy is selected from the pinned Fairphone Qualcomm sources recorded in
`provenance.json`. Each imported file retains its original licence. It is
maintained source, separate from generated stock files and kernel artifacts.

The selection covers the declared boot, display, credential, power, thermal,
USB and basic hardware services. Manufacturing applications, diagnostics,
unselected radio/camera services and their test exceptions are excluded.
Shared type declarations do not imply that the corresponding service is
installed. Adding a service requires reviewing both its activation and grants.

Downstream adaptations:

- Bind hardware grants to the service domain; preserve required HAL IPC
  attributes. Do not grant hardware access through a HAL attribute by default.
- Keep module insertion in `vendor_modprobe`, matching the explicit init
  execution domain. Vendor init retains the additional traversal needed to
  restore mounted persist labels, rather than module-loading capabilities.
- Use Android 17's genfs ownership for the Type-C class and sleep control.
  The USB service receives class-directory enumeration and link traversal.
- Remove legacy HBTP power access, whose implementation is absent from the
  selected common AIDL power service.
- Use platform init/ueventd permissions where they already implement selected
  operations. Omitted firmware-handler transitions must be revisited if the
  product activates those handlers.
- Retain enforcing mode, compatibility tests and neverallow checks. USER
  compilation and checks for absent module-loading, kernel scheduling, IPA
  writes, unused WIGIG setters, HBTP access and display executable memory pass.

This is a development integration baseline. Native compilation does not prove
hardware functionality or complete least privilege. Remaining performance
backend, firmware-handler and auxiliary-service decisions must be checked
against the final selected product and first-boot evidence. Do not resolve a
failure by importing stock policy wholesale or adding a permissive domain.
