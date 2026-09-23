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
- Omit the peripheral manager's binder call into the WLAN service domain; that
  service is not installed.
- Label the stock FP6 fingerprint wrapper (`android.hardware.biometrics.
  fingerprint-service`) `hal_fingerprint_default_exec`, as on the stock image;
  its FocalTech node and QSEECom grants are an enforcing-mode follow-up.
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

## Recovery ownership

The recovery executable uses the boot-control service over IPC; it does not
link the legacy Qualcomm updater. Raw SCSI/BSG access and CAP_SYS_RAWIO are
therefore not granted to recovery by this device supplement. Boot control keeps
BSG access and CAP_SYS_RAWIO for the selected UFS implementation. Legacy sg
character-device access is omitted, while the sysfs discovery used by
`gpt-utils` remains. Read/write access to recovery UI and firmware resources is
owned by the operation's actual init, ueventd or recovery domain.

## Updating upstream policy

The source of truth is the exact Fairphone-published Qualcomm repositories and
revisions in `provenance.json`; local files are a reviewed subset with downstream
adaptations. Do not track a moving branch in the build or include the entire
upstream SEPolicy.mk as a shortcut. That would activate policy for services the
product does not install and could undo platform ownership and domain scoping.

For each upstream refresh:

1. Fetch the two declared repositories into review checkouts, keeping the old
   revisions available. Verify each recorded original file hash with
   `git show OLD_REVISION:SOURCE_PATH | sha256sum` and each local derived hash
   before changing the baseline.
2. Review `git diff OLD_REVISION NEW_REVISION -- SOURCE_PATH` for every selected
   file. Also inspect the full upstream diff/name-status, including shared
   macros, attributes, contexts and selection makefiles: a new security fix may
   live outside the current file selection. Check both repositories together.
3. Merge relevant changes against the old upstream file and our derived file;
   do not overwrite downstream adaptations. Trace each added allow to the
   current provider/resource. Include new files only when their consumers or
   shared definitions are required. Preserve licences and original hashes.
4. Update upstream revisions/source hashes and resulting local hashes in
   provenance.json. Review removals/renames explicitly. Publish a new device
   commit and update the pinned build environment/project map.
5. Compile USER and recovery policy with neverallows enabled, inspect effective
   grants and negative privilege checks, and repeat affected device tests.
   Build success alone does not establish runtime or least-privilege acceptance.

This preserves upstream traceability without importing unused policy. A shared
DiamaneOS policy repository becomes useful when multiple devices genuinely share
this maintained subset; it is not required merely to reduce this folder's size.
