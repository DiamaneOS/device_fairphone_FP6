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
- Drop the QSEECom AIDL proxy (`vendor.qti.hardware.qseecom@1.0-service`):
  `hal_qseecom.te` and its file and service contexts are removed, and the
  service is not selected. No installed file looks it up; KeyMint, Gatekeeper,
  the fingerprint module and `qseecomd` use `libQSEEComAPI` directly. The
  kernel `qseecom_proxy.ko` is a different component and stays.
- Label the FP6 fingerprint service (`android.hardware.biometrics.
  fingerprint-service.fp6`) `hal_fingerprint_default_exec`, the domain of the
  stock wrapper it replaces.
- Allow QRTR sockets (`qipcrtr_socket`, no ioctls) for `vendor_per_mgr`: its
  QMI libraries open AF_QIPCRTR sockets, which the upstream rules only grant
  as generic `socket`.
- `fp6/` holds device-owned grants, each bound to the service that showed the
  denial on a permissive boot:
  - Sensors: `sensors.te` (sscrpcd) and `hal_sensors_default.te` are reduced
    from the pinned Qualcomm files. They use FastRPC only through the secure
    node (`vendor_xdsp_device`), QRTR without ioctls, and have no capabilities,
    diag, sysrq, SLPI/SSR sysfs, HID or persist writes from the HAL; it reads
    persist/sensors. `sensors_list.txt`, the sensor types the HAL waits for at
    start, has its own label (`vendor_persist_sensors_list_file`), and init
    copies it from vendor at every boot with the stock list minus the hall
    sensor: the HAL never removes an entry, and the stock list names a hall
    sensor this build's sensor core never reports, which held the HAL, and
    system_server with it, for 8 s at every boot. Stock lets the HAL write all
    of persist/sensors instead.
  - Graphics: the in-process Adreno driver reads the GPU model (the Qualcomm
    domain grant) and its read-only graphics properties; `libllvm-qgl.so` is a
    same-process HAL library; the composer keeps state in
    `/data/vendor/display`. The gralloc properties (`vendor.gralloc.*`) are
    readable by the processes that map buffers, including the camera
    provider, cameraserver and mediaserver; stock lets every domain read
    them.
  - Extcon: the DisplayPort and audio-codec extcon devices are
    `sysfs_extcon`, which system_server reads to detect DisplayPort (HDMI)
    and wired headsets. The imported per-index `vendor_sysfs_graphics` lines
    for `msm-ext-disp` are removed from `qva-common/genfs_contexts`: extcon
    numbers follow probe order, so they labelled whichever device got the
    number, and no vendor service reads extcon. Stock instead lets
    system_server write all of `vendor_sysfs_graphics`.
  - Fingerprint: the FocalTech node is `ff_device` (stock label), with QSEECom
    and its heaps for the fingerprint HAL, each limited to the permissions the
    module used. The module registers a debug binder service with no caller
    check at every start and fails without it; our HAL keeps it inside its own
    process (`fingerprint/ModuleFactoryService.h`), and it stays unlabelled,
    so the platform neverallow on `default_android_service` forbids granting
    it. A neverallow keeps the HAL from opening files in `vendor_data_file`,
    where the module's configuration and dumps would live.
  - Read-only SoC, thermal-zone and remote-processor names for the thermal
    and peripheral-manager services; `/dev/wlan` and the driver
    version property for the Wi-Fi HAL; the vendor patch level for KeyMint;
    vendor properties set from vendor init scripts.
  - SoC serial number: `/sys/devices/soc0/serial_number` is
    `vendor_sysfs_soc_serial` (`soc_serial.te`), not the soc0 directory's
    `vendor_sysfs_soc`. No shipped program reads it (only kernel code uses
    the serial), so no vendor rule grants it: the composer's imported read of
    all sysfs excludes it and the thermal engine no longer reads
    `vendor_sysfs_soc` files (it reads only the public `soc_id` and
    `hw_platform`). The type must keep `sysfs_type`, which the platform lets
    apexd, init, ueventd, vendor_init, vold, the Bluetooth, Wi-Fi, supplicant
    and radio HALs and tee read; ueventd makes the node 0400 root, so of those
    only the root platform daemons can open it. A neverallow keeps every other
    domain off it.
  - Thermal HAL: runs as system with no capabilities
    (`power/init.fp6.thermal.rc`); ueventd gives group system the two trip
    nodes it writes (`trip_point_1_temp` and `trip_point_1_hyst` of each
    thermal zone). Its domain is unchanged.
  - The USB speed node is `sysfs_udc` (in the imported file_contexts and
    genfs_contexts), not the factory-test type `fp_mmitest_sysfs`, which also
    covers camera calibration and download mode.
  - The UFS LUN 0 block directory is `sysfs_devices_block`, which init may
    only read. The one node platform init.rc writes there, the root disk's
    `queue/discard_max_bytes`, is `vendor_sysfs_ufs_discard_max` (`storage.te`),
    writable by init only. The NFC controller's wakeup source is
    `sysfs_wakeup`.
  - Tuning: `init.fp6.perf.rc` writes WALT and VM sysctls that the platform
    does not label. `/proc/sys/walt` is `vendor_proc_walt`, `swappiness` and
    `min_free_kbytes` are `vendor_proc_vm_tuning`, `compaction_proactiveness`
    is `vendor_proc_compaction` (`perf.te`). Only vendor_init writes them; the
    power HAL reaches `sched_boost` through fixed property triggers (see
    [Power](#power)). vendor_init also writes the platform `proc_sched` and
    `proc_watermark_scale_factor` sysctls; it still has no write on generic
    `proc`.
  - camera.ko's CCI probe asks to raise the CCI IRQ thread from the kernel
    default SCHED_FIFO 50 to 99 from `vendor_modprobe`; this stays denied
    (non-fatal); `vendor_modprobe` has no `sys_nice` and no kernel task
    control.
  - Wi-Fi: `/proc/sys/net/ipv4/tcp_limit_output_bytes` is
    `vendor_proc_tcp_limit_output_bytes`, read and written only by the Wi-Fi
    HAL for high-throughput TCP tuning; the rest of `/proc/sys/net` stays
    read-only to it. `/data/vendor/tombstones` is no longer labelled as a
    whole (only `rfs/`), so the platform `tombstones/wifi` label for the HAL's
    ring-buffer logs applies.
  - The genfs lines for these tuning nodes are in `fp6/genfs_contexts`.
  - Touch: the controller's double-tap wake switch (`gesture_wakeup`) is
    `vendor_sysfs_touch_gesture`, not `fp_mmitest_sysfs` like the rest of the
    touch device. Only the power HAL writes it (Mode::DOUBLE_TAP_TO_WAKE); it
    may search the device directory but not use its factory-test files.
  - Vibrator: the Awinic haptics nodes the vibrator HAL writes (`activate`,
    `brightness`, `duration`, `gain`, `loop`, `seq`) are
    `vendor_sysfs_aw_vibrator`. The imported `hal_vibrator_default.te` no
    longer lets the HAL write every `sysfs_leds` file, so raw waveform
    streaming (`rtp`), calibration and the other LEDs stay read-only to it.
    The imported files also lose the HAL's `input_device` access (it no
    longer probes `/dev/input` when the Awinic chip is present, and its
    service drops the `input` group), so it can neither read touch and key
    events nor drive the chip's force-feedback input node. The grants for the
    absent `qcom-haptics` sysfs node and persist haptics calibration are
    removed too.
  Not granted: `qseecomd` on the raw UFS LUN 0 node (a platform neverallow on
  `device`; RPMB and LUN 4 have their own labels) and `fsck` on `vm-bootsys`
  (its fstab line is gone: nothing on the FP6 mounts `/product/vm-system`). The modem's `study` partition has
  the stock `vendor_modem_efs_partition_device` label
  (`vendor-volcano/file_contexts`), so `rmt_storage` can serve it with its
  existing grant.
- `product-private/property_contexts` keeps `debug.disable_screen_decorations`
  on `vendor_display_notch_prop`, which SystemUI cannot read: the read falls
  back to false, so the privacy dot cannot be switched off with `adb setprop`.
- `system-ext-private/system_app.te` lets Settings read only
  `ro.vendor.build.security_patch` (`vendor_security_patch_level_prop`) for the
  "Vendor security update" row; the platform grants it to shell, keystore and
  vendor_init only.
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

## Denied on purpose

Denials seen on enforcing boots that stay denied, each checked against what
the process does without the access. Denials covered in the sections above
are not repeated.

- `hal_sensors_default`, `property_socket` write: the proximity raw-data
  factory property; not audited (`fp6/hal_sensors_default.te`).
- `hal_sensors_default`, append to `sensors_list.txt`: init owns the list.
- `system_server`, extcon cable names of the EUD debug port (`sysfs`): no
  cable type the framework uses.
- `system_server` (InputReader), `max_brightness` of the haptics LED device
  (`sysfs_leds`): it sits next to the chip's input device; reading it would
  let InputReader treat the vibrator as a light.
- `system_server`, `surfaceflinger`, `mediaserver`, `mediaswcodec` and apps,
  `libubwcp.so` (`vendor_file`): the source-built mapper tries to load it on
  first use, but UBWC-P support is compiled out, so it would never be used.
- `hal_camera_default`, `ro.vendor.qti.soc_id` (`vendor_soc_id_prop`): not set
  on this build; CamX uses the public SoC id file. As stock.
- `hal_bluetooth_default`, `hal_camera_default`, `vendor_hal_gnss_qti`,
  `ro.vendor.qti.va_aosp.support` and `va_odm.support`
  (`vendor_exported_system_prop`, `vendor_exported_odm_prop`): not set on this
  build, so a denied read returns the same default.
- `hal_audio_default`, `rild`, `persist.vendor.pd_locater_debug`
  (`vendor_pd_locater_dbg_prop`): a debug switch, off when unreadable; stock
  grants it only to its PD mapper, and no domain reads it here.
- `vendor_nicmd`, `property_socket` write and an `init.svc.*` read
  (`init_service_status_private_prop`): the SHS and QMI-priority helpers it
  starts and checks are not installed.
- `vendor_nicmd`, `netlink_route_socket` `nlmsg_readpriv`: seen after a Wi-Fi
  change; stock does not grant it either.
- `vendor_nicmd`, `rawip_socket` `create`: seen after a Wi-Fi change; stock does
  not grant it either, and Wi-Fi calling works without it.
- `fp6_iwlan_app` (qtidataservices) and `mediacodec` (the Codec2 image-texture
  filter), reads of `default_prop` and `zygote_config_prop`: generic platform
  properties (verified-boot partition info, build UUIDs, runtime flags) a
  denied read returns as unset; neither needs them.
- `hal_camera_default`, finding `vendor.tcl.camera.algoservice.ITctCameraAlgoService`
  (`vendor_tct_camera_algo_service`): once per camera open; the TCL algorithm
  service is not installed (see Camera), and CamX continues without it.
- `tee` (qseecomd), opening the GPT, XBL and boot block devices and the BSG
  nodes of the other UFS LUNs: it probes them at start; stock grants read on
  the block devices but never open, and labels none of those BSG nodes.
- `fp6_callaudio_app`, lookups of `content_capture`, `gpu` and `netstats`:
  framework start-up probes the bridge does not need.
- `platform_app` and `system_app`, the absent Google wireless-charger service;
  SystemUI, `persist.bluetooth.leaudio_dynamic_switcher.mode`: platform app
  policy, nothing added for apps.
- `untrusted_app`: sandboxed Google Play and other apps probing the device
  (adb properties, `/proc`, `/sys`, `selinuxfs`, the root and `/dev`
  directories, the wallet property, cgroups). Intended.
- `shell` and `su`: only the commands used to capture logs.

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

## Audio

`audio/adsprpcd.te` and `audio/hal_audio_default.te` are downstream-authored
adaptations of the matching Qualcomm `sepolicy_vndr` files published by Fairphone
at `67fa928299a49374a55281969fee357884c86890`. Their original licence headers,
source hashes and derived hashes are retained in `provenance.json`. The audio
file contexts and init-directory rules are authored by DiamaneOS.

The policy deliberately omits upstream diagnostic-device access, sensor persist
writes, voice-UI sockets, QTR SDK access, broad HAL-attribute grants, vendor
Binder use and DSP restart controls (`vendor.audio.crash.trigger` is unset). QRTR
is granted narrowly: `hal_audio_default` may create and use its own
`qipcrtr_socket` (create, read, write, getattr, setopt; no ioctl, bind or
connect) for the audio-PD lookup and restart notifications of GPR and GSL.
SELinux cannot filter QRTR by service, so the HAL can send QMI to any service.
AGM and PAL open the sound-card state node read-write, so the HAL may also write
it. PAL's wake locks (sound trigger only, not shipped) stay denied. The audio
policy confines device access to the audio service domains, keeps amplifier
factory calibration read-only and restricts the PAL sleep-monitor extended ioctl
grant to activity reporting (`0x5201`). AudioReach receives its AGM device,
runtime audio directory, selected allocator and sound-card state access. The
FastRPC listener receives only its DSP transport, firmware/RFSA, audio-DSP state
and DSP-service lookup.

The stock primary HAL registers the PAL and AGM HIDL services (`IPAL`, `IAGM`)
in its own process. `qva-common/hwservice_contexts` labels them
`vendor_hal_audio_internal_hwservice` (`audio/hwservice.te`) instead of
`hal_audio_hwservice`, so only `hal_audio_default` may register and look them
up; audioserver, system_server and the Bluetooth stack, which are hal_audio
clients, cannot reach them.

The kernel may not search `/mnt/vendor` (a platform neverallow), so it cannot
follow the amplifier calibration link into persist; the amplifier then uses its
default calibration. These rules are not yet runtime-qualified under enforcing
mode; collect denials on the device before adding any grant.

## Modem

The modem (MSS) is booted by the stock peripheral manager through its
remoteproc character device, which ueventd gives to `system` owner-only.
`modem/ssr_setup.te` adds the stock subsystem-restart helper domain
`vendor_ssr_setup`. It is derived from Qualcomm's policy as compiled into the
stock FP6 image until the Qualcomm `generic/vendor/common/ssr_setup.te` at
`67fa928299a49374a55281969fee357884c86890` is fetched; then its licence header
and hashes are recorded in `provenance.json`. It lists `/sys/class/remoteproc`,
reads processor names and writes the `recovery` switches only
(`vendor_sysfs_ssr_toggle`), and reads `persist.vendor.ssr.`. Omitted stock
grants: writes to `vendor_sysfs_ssr` files and links (the legacy
`msm_subsys` restart_level interface, absent on this kernel) and reads of
`vendor_sysfs_data`. `modem/file_contexts` labels `/vendor/bin/ssr_setup`; the
MSS, ADSP, CDSP and WPSS recovery switches are in `vendor-volcano/file_contexts`.
The full RAM dump collector (`vendor_subsystem_ramdump`) is not installed and
has no policy. Remote-processor error logs are not written to /data (no
`/data/vendor/tombstones/rfs`, and the file server below serves no ramdump
paths). These rules are not yet runtime-qualified under enforcing mode.

The modem's TFTP file server and the protection-domain mapper are the
open-source linux-msm `tqftpserv` (DiamaneOS fork with memory and path fixes,
unlink and truncation) and `pd-mapper` (BSD-3-Clause, built from
`vendor/qcom/opensource`). They replace Qualcomm's `tftp_server` and
`pd-mapper`: the imported `vendor-common/rfs_access.te` and `pd_services.te`
and their `vendor-common/file_contexts` lines are removed, and so are the
`/vendor/rfs` links into persist that only `tftp_server` used. Each service
runs as its own vendor user (`modem/config.fs`) with no capabilities, no
network, no binder and no properties; the stock daemons held setuid, setgid,
setpcap and net_bind_service (and chown for `tftp_server`).

- `modem/tqftpserv.te` (`vendor_tqftpserv`): QRTR sockets (create, connect,
  getattr, read, setopt, write; no ioctl or bind), listing
  `/sys/class/remoteproc` and reading each processor's `firmware` attribute
  (`vendor_sysfs_rproc_firmware`, `modem/file_contexts`), and its read-write
  directory `/data/vendor/tmp/tqftpserv` (`vendor_tqftpserv_data_file`: open,
  create, read, write and delete files; no rename, link, attribute change or
  subdirectory). The modem asks for `modem_pr/...` read-only files, served
  through the `/vendor/firmware/modem_pr` link to the modem partition, writes
  `server_check.txt` and `mcfg.tmp`, and deletes `mcfg.tmp` (the TFTP
  "unlink" option). tqftpserv deletes only files in that directory and follows
  no symbolic links there. Unlike `tftp_server`, nothing goes to persist, so a
  factory reset clears these files. Its probe of the
  kernel's firmware search path (`vendor_sysfs_firmware_path`,
  `modem/genfs_contexts`) stays denied and is not audited: on the FP6 that path
  is a comma-separated list tqftpserv cannot use. A neverallow keeps other
  vendor services from changing its files.
- `modem/pd_mapper.te` (`vendor_pd_mapper`): QRTR sockets (create, getattr,
  read, setopt, write) and the same remoteproc listing and `firmware`
  attribute. Its domain lists (`/vendor/firmware/*.jsn`, links to the modem
  partition) and the modem partition itself are readable through the
  platform's vendor-file rules, as for tqftpserv. Stock also read
  `vendor_sysfs_data` and `persist.vendor.pd_locater_debug` and used the
  legacy IPC router socket with its ioctl; none of that is granted.
- Both: `kmsg_debug_device` write on debuggable builds only, for
  `stdio_to_kmsg` (their logs go to the kernel log); stdio's fstat and terminal
  ioctl on it stay denied and are not audited.

Both read the modem partition, which is mounted for `system` (uid and gid
1000, `fstab.qcom`), through the `system` supplementary group; SELinux limits
what they can read. QRTR has no per-service access control, so either service
can reach any QMI service; the `telephony/qrtr.te` inventory lists
`vendor_tqftpserv` and `vendor_pd_mapper`.

## Bluetooth

`bluetooth/hal_bluetooth_default.te` is drafted by DiamaneOS from the stock
compiled vendor policy of FP6.QREL.16.100.0 (`vendor_sepolicy.cil`, rules for
`hal_bluetooth_default` and `hal_bluetooth`), for the stock Qualcomm HCI
service `android.hardware.bluetooth@1.1-service-qti`. Our own service
(`bluetooth/service.cpp`) replaces that binary and needs the same access: it
reads the chip's compatible string from btpower, sets the SoC name and
registers the stock HCI implementation. The policy is to be reduced from
Qualcomm `generic/vendor/common/hal_bluetooth.te` (or `hal_bluetooth_default.te`) at the pinned
`sepolicy_vndr` revision once that file is fetched; then record it in
`provenance.json`. `bluetooth/file_contexts` gives our service the stock
service's label (stock `vendor_file_contexts` line 1125).

The policy grants the btpower node, read access to the Bluetooth firmware
partition and to the Bluetooth persist directory, read access to the Bluetooth
vendor properties and read-only SoC identification. The HAL may set only the
five properties it writes at run time (SoC name, scram.enabled, a generated
address and two crash counters); `bluetooth/property_contexts` gives them the
vendor-internal type `vendor_bluetooth_hal_state_prop`, so no platform domain
or app can read the address once enforcing. The HCI UART (`hci_attach_dev`)
comes from platform policy, as in stock. The policy omits persist writes, the
stock QRTR sockets (only used for the modem-NV address query, which is off),
`/data/vendor/bluetooth`, diag, the FM radio device, the ssgtzd socket, TPI and
Xpan service registration and HSUART tracing.

The FM, ANT, SAR, config-store and TPI libraries the stock service linked are
not installed. The imported hwservice_contexts map `com.dsi.ant::IAnt`,
`com.qualcomm.qti.ant::IAntHci` (vendor-common lines 32-33) and
`vendor.qti.hardware.bluetooth_sar::IBluetoothSar` (qva-common line 65) to
`hal_bluetooth_hwservice`, which the HAL may add. Keep those interfaces out of
the device VINTF manifest.

Before enforcing mode:
- `/dev/btfmcodec_dev` and `/dev/bt_cp_ctrl` share `hci_attach_dev` with
  `/dev/ttyHS0`, and `/dev/btfmslim` shares `vendor_bt_device` with
  `/dev/btpower` (vendor-common/file_contexts lines 65, 152-154). They stay
  root-only through ueventd today. Give them their own device types that the
  HAL is not granted.
- Label the rfkill and `hs_uart_operation` sysfs nodes
  `sysfs_bluetooth_writable` with device-path genfs rules, and move their
  ownership from index-based init lines (`rfkill0`) to ueventd device-path
  lines, once the phone gives the btpower device path.
- If `hal_audio_default` shows a read of `persist.vendor.qcom.bluetooth.soc`,
  grant it `get_prop` on `vendor_bluetooth_hal_state_prop` (stock lets the
  audio HAL read the Bluetooth properties, vendor_sepolicy.cil 4638, 7863).

Not yet runtime-qualified under enforcing mode.

## NFC

`nfc/file_contexts` labels the stock Samsung NFC HAL
(`android.hardware.nfc-service.sec`) `hal_nfc_default_exec` and the controller
node `/dev/sec-nfc` `nfc_device`, the labels in the stock compiled
`vendor_file_contexts`. The domain `hal_nfc_default` and its controller access
come from AOSP policy (`hal_server_domain(hal_nfc_default, hal_nfc)`; `hal_nfc`
may read and write `nfc_device`). No device grant is added yet.

The Qualcomm `hal_nfc_default.te`, `nfc.te`, `nqnfcinfo.te`,
`hal_secure_element_default.te` and `secure_element.te` are not imported. Most
of their grants are for NXP/QTI parts that are not installed or not used by
the Samsung HAL: HIDL registration (the HAL registers only AIDL
`INfc/default`), the NXP NFC service, `vendor.qti.nfc.*` properties, the
`ssgtzd` socket, `/firmware` and `/data/vendor/nfc`. The controller firmware
in `/vendor/firmware` needs no grant: vendor domains may read
`vendor_file_type` through platform policy.

The HAL's `property_set` of `vendor.nfc.fw.version` (`vendor_nfc_prop`) stays
denied: `nfc_nci_sec.so` ignores the result and never reads it back, and the
only stock reader is the factory MMI app. If the firmware version is wanted in
bug reports, the minimal rule is
`allow hal_nfc_default vendor_nfc_prop:property_service set;` in
`nfc/hal_nfc_default.te`. The HAL's read of `persist.sys.factory.mode`
(`system_prop`) also stays denied, which keeps it on the normal firmware path.
`/data/nfc`, which platform init.rc creates for the NFC stack's state, is
`nfc_data_file` (`system-ext-private/file_contexts`, as in stock).

## GNSS

`gnss/hal_gnss_qti.te` confines the stock Qualcomm GNSS HAL
(`android.hardware.gnss-aidl-service-qti`) in `vendor_hal_gnss_qti`. It is a downstream
draft reduced from the stock compiled policy (vendor_sepolicy.cil lines 4914-4953 and
8128-8141) to the selected closure. It will be replaced by a reduction of the matching
Qualcomm `sepolicy_vndr` file at `67fa928299a49374a55281969fee357884c86890` once that file
is fetched; provenance is recorded then.

The service is a HAL server for `hal_gnss` and a client of `hal_health`. It reaches the modem
over QRTR (`qipcrtr_socket`, no ioctls; stock grants this to the `hal_gnss` attribute,
here it is bound to the domain), keeps small state files in `/data/vendor/location` and
serves its IPC socket in `/dev/socket/location`. It reads the modem version file from
`/vendor/firmware_mnt` and the public SoC id and board type. `gnss/genfs_contexts` labels
`/sys/devices/soc0/hw_platform` `vendor_sysfs_public`, so the HAL does not need read access
to the rest of `vendor_sysfs_soc`, which includes the SoC serial number.

The policy omits the peripheral manager and vndbinder, the location daemons' sockets
(loc_launcher, XTRA, LOWI, Wi-Fi crowdsourcing, sensor, correction, engine and Skyhook
services), RIL and SSG sockets, MHI sysfs and QMS/AON properties: none of those peers is
installed. It adds no servicemanager rule; platform policy covers AIDL HAL registration, as
for the gatekeeper and keymint HALs. Not yet runtime-qualified under enforcing mode; collect
denials on the device before adding any grant.

## Time keeping

`timekeep/` holds our time daemon, which replaces Qualcomm's `time_daemon` (its imported
policy, file label and `tee` socket rule are removed). A one-shot restore step
(`vendor_timekeepd_restore`) sets the clock at post-fs-data from the RTC counter plus the
saved offset; it is the only domain with `sys_time` and cannot write the offset file. The
long-running `vendor_timekeepd` holds no capability: it searches `/sys/class/rtc`, reads
the RTC counter file `since_epoch` (its own genfs type) and replaces files in
`/data/vendor/timekeepd`. Neither has sockets, binder, properties, `/dev/rtc0` or persist,
and a neverallow keeps other vendor domains from writing the offset file.

## Power

The power HAL is LineageOS's libperfmgr (`android.hardware.power-service.lineage-libperfmgr`,
device `power/`) in the platform `hal_power_default` domain (`fp6/power.te`). It replaces
Qualcomm's perf2 daemon, which ran as root in `vendor_hal_perf_default` with setuid, kill and
sys_nice, could read and write every app's `/proc` files and reached about 25 sysfs types. That
domain (`qva-common/hal_perf_default.te`), its file and service contexts, and the perf client
grants of the composer, SurfaceFlinger, the camera and the old power HAL are removed.

- Process: user and group system, CAP_SYS_NICE only (`power/init.fp6.power.rc` overrides the
  module's root service); no readproc group, as the HAL reads no `/proc/<pid>` entries.
- Nodes: the HAL may write, not read, `scaling_min_freq` and `scaling_max_freq` of the three
  CPU policies (`vendor_sysfs_cpufreq_limit`, `fp6/genfs_contexts`), the GPU devfreq
  `min_freq` and `max_freq` and the GPU wake trigger `touch_wake` (`vendor_sysfs_kgsl_limit`,
  `fp6/file_contexts`), and the touch gesture switch (`touch.te`). Init chowns exactly these
  to system; the rest of the CPU and GPU sysfs stays root-owned with its platform or Qualcomm
  label. The thermal engine keeps the read and write it had on these nodes under their old
  labels, and system_server keeps read on the CPU nodes (its CPU monitor, debuggable builds).
- `sched_boost`: `/proc/sys/walt/sched_boost` is root-only and a sysctl cannot be chowned.
  The HAL may set only `vendor.powerhal.sched_boost` (`vendor_power_sched_boost_prop`, values
  0, 1 and 2), and vendor init writes the matching fixed value. The other `vendor.powerhal.*`
  properties (`vendor_power_prop`) are switches the HAL reads and vendor init sets;
  `vendor.powerhal.sched_boost.enable=false` turns the `sched_boost` part of the hints off.
- ADPF: setsched on apps, SurfaceFlinger and system_server, with CAP_SYS_NICE, to set uclamp
  on hint-session threads. The domain is an `mlstrustedsubject` because the platform MLS
  constraint on setsched requires equal levels and apps run with categories; setsched is its
  only access to app processes. It is a thermal HAL client for the throttling state.
- `libqti-perfd-client` (`power/libqti-perfd-client`) is a source stand-in for the closed
  client library the stock camera and SDM extension load by name. It forwards only the
  camera's open, close and snapshot hints, as CAMERA_LAUNCH and CAMERA_SHOT boosts of at most
  5 s (a hint held until release at most 2 s, each ended early when CamX releases it); every
  other call does nothing. It is `vendor_file`: no app process loads it.
- Clients: the platform's (system_server, SurfaceFlinger, apps' hint sessions through
  system_server) and, the only vendor one, the camera provider
  (`camera/hal_camera_default.te`). A client reaches every IPower method: boosts, modes such
  as SUSTAINED_PERFORMANCE, EXPENSIVE_RENDERING or DOUBLE_TAP_TO_WAKE, and ADPF hint sessions.
  Their effects stay within the grants above: frequency floors and caps, the three
  `sched_boost` values, the GPU wake trigger, tap-to-wake and uclamp on session threads.
- Not granted: reads of the nodes (dumpsys shows request indexes, not values), the debug
  configuration in `/data/vendor/etc` (`vendor.powerhal.config.debug`), the Pixel-only
  `/proc/vendor_sched`, the display `idle_state` nodes (the Qualcomm display driver has
  none; `vendor.powerhal.disp.idle_support=false`) and setsched on the composer (add it only
  if SurfaceFlinger puts composer threads in its hint session).
- Kept as declarations only: Qualcomm's HIDL perf hwservice types and contexts and the
  `vendor_hal_perf` attributes.

These rules are not yet built or runtime-qualified.

## Power stats

The power stats HAL (`android.hardware.power.stats-service.fp6`, device `power/stats`) runs
in the platform `hal_power_stats_default` domain (`fp6/power_stats.te`); its clients are the
platform's IPowerStats clients. Stock FP6 ships no power stats HAL.

- Process: its own vendor user and group `vendor_powerstats` (`power/config.fs`), no
  supplementary groups, no capabilities.
- Device: `/dev/stats`, the qcom_stats driver's sleep counter node, is
  `vendor_qcom_stats_device` (`fp6/file_contexts`) and 0400 for the HAL's user
  (`boot/ueventd.rc`); without the entry it would be root's with the generic `device` label.
  The HAL may open, read and ioctl it, nothing else; a neverallow keeps other vendor domains
  off it and the HAL from writing it.
- ioctls: an `allowxperm` limits the HAL to the seven commands it uses: modem, WPSS, ADSP
  and CDSP sleep and the AOSD, CXSD and DDR records. The driver's APSS, island, SLPI, GPU and
  display commands read the wrong subsystem entries (its table gained entries the command
  mapping does not follow), and DDR frequency residency sends a request to the AOSS on each
  read; none of them is allowed.
- Not used: debugfs (the driver's text files, the CPU-subsystem sleep stats and the sysmon
  stats exist only there), sysfs, properties, and energy meters: the FP6 has no on-device
  power monitor.

These rules are not yet built or runtime-qualified.

## Telephony

`telephony/rild.te`, `telephony/nicmd.te` and `telephony/qtelephony.te` are downstream
reductions of the Qualcomm `sepolicy_vndr` files published by Fairphone at
`67fa928299a49374a55281969fee357884c86890` (`generic/vendor/common/rild.te`,
`qva/vendor/common/rild.te`, `generic/vendor/volcano/rild.te`,
`generic/vendor/common/hal_telephony.te`, `generic/vendor/common/nicmd.te`,
`generic/vendor/common/qtelephony.te`); `telephony-system-ext/vendor_qtelephony.te` and
`telephony-system-ext/seapp_contexts` reduce `generic/private/qtelephony.te`,
`generic/private/seapp_contexts` and `generic/product/private/seapp_contexts` of
`device/qcom/sepolicy` at `fea28791cc44cce15c7bd4cd94722796e4644735`. The file contexts
are the stock labels. `telephony/service.te`, `fp6_callaudio_app.te`, the
`fp6_*_app` domains and the relabelling of IQcRilAudio and IUimLpa in
`vendor-common/service_contexts` are downstream.

The stock radio daemon runs in the platform `rild` domain. It may add only the four
Qualcomm radio services the device declares (IMS and radio config under
`vendor_hal_telephony_service2`, call audio and LPA under their own types). It is not
a `binderservicedomain`; each selected app client has an explicit binder grant. The
secure-element HAL, unrelated data-factory services, diag, the legacy
IPC-router ioctls, the QCRIL client socket and executing vendor tools are not granted.
The radio daemon and nicmd may use TIPC sockets with each other, as on stock: the data
module's DSI layer waits for nicmd over TIPC before it allows any data call. No other
domain may create a TIPC socket (neverallow in `telephony/rild.te`), and the kernel builds
TIPC for local IPC with network bearer creation blocked, without UDP, crypto or diag modules.
nicmd keeps its netlink, QRTR, rmnet ioctl and network-wrapper access, datagram sockets
for interface ioctls and its init-created recovery file. It may `node_bind` TCP and UDP
sockets, to reserve the ephemeral ports the modem's embedded clients use, and read the
public SoC id for data target detection. It is not a `netdomain` (no TCP connect, no
`name_bind`); its remote-processor probe (`vendor_sysfs_ssr`) stays denied because the
result is unused on this SoC. The SHS, QMI-priority and performance helpers are not
installed and not granted.

The stock IMS and IWLAN/certificate apps retain Fairphone's signature and a
signer-specific seinfo, with privileged placement and package/process selectors.
The IMS app runs in `vendor_qtelephony`; the coupled IWLAN/certificate frontend
runs in `fp6_iwlan_app` with explicit radio Binder and certificate QRTR access.
Package selectors label its data separately from the shared-process selector.
The inactive LPA, its download grants and app-domain policy are omitted; the
native IUimLpa interface remains because the selected radio binary depends on it.
These presigned apps retain OEM update trust; release signing and update policy
remain separate trust decisions. Collect denials before adding any grant.

Our own call-audio bridge (`de.diamaneos.callaudio`, `callaudio/`) replaces the stock
QtiTelephonyService. Its signer, package name and privileged placement select the domain, and it runs in
`fp6_callaudio_app`: it may find only IQcRilAudio, the audio server and the activity
manager, make binder calls with the radio daemon, and create no network or QRTR sockets.

## Camera

`camera/` holds the policy for the stock CamX/CHI provider
(`vendor.qti.camera.provider-service_64`, platform domain `hal_camera_default`). The
grants are drafted from the stock FP6 compiled vendor policy and bound to
`hal_camera_default`, not to the `hal_camera` attributes. They cover the ToF node, UBWC-P,
FastRPC with read-only opens (the secure node for the ADSP sensors PD of the CamX sensor
direct channel, the non-secure node for the CDSP offloads, `/vendor/dsp`), Qualcomm
DMA-BUF heaps (display heap allocation ioctl only), SoC/camera/JPEG/DDR identification,
QRTR sockets without ioctls for the gyro QMI client, the thermal-engine client socket, the
provider's own vndbinder open, `/data/vendor/camera`, read-only factory calibration in
`/mnt/vendor/persist/camera` and vendor camera properties. For streaming the provider is a client of the graphics allocator (buffer
allocation and IMapper) through `hal_client_domain`, the only form the platform
neverallows allow for the allocator service lookups. The membership also lets it find the
mapper services, execute `same_process_hal_file` (the passthrough IMapper), call
servicemanager and share memfds with the allocator, and, as a `halclientdomain`, call
hwservicemanager, read `hwservicemanager_prop` and find `hidl_manager_hwservice`. CamX's
perf hints go to our `libqti-perfd-client` (device `power/`), never to the perf2 service
(with the stock library its perf2 lookups were denied at camera open and close, each waiting
about a second). The library forwards the open, close and snapshot hints to the power HAL as
time-limited boosts, so the provider is a power HAL client through
`hal_client_domain(hal_camera_default, hal_power)`, as on Pixels: the platform neverallow on
`hal_power_service` lookups allows only that form. The membership lets it find the power
service, make binder calls with the power HAL both ways (the HAL never calls back), share
memfds with it and find the HIDL power service, which nothing registers. SELinux cannot limit
the grant to the camera boosts: it covers the whole IPower interface (see [Power](#power)).
It may use the
composer's release fences, read the public SoC id and
search `/sys/devices/soc0` for the per-part files (`num_subset_parts` is labelled in
`camera/genfs_contexts`). Denied: the display QService and display-config lookups
(IDisplayConfig would expose brightness, power mode and writeback capture),
IPostProcService registration and the property-area listing CamX does at start (silent
under enforcing), and the offline camera service registration (in no VINTF manifest, not
audited). Unlike the sensors HAL, the camera may need the non-secure FastRPC node:
the kernel runs the CDSP as a non-secure channel. Only one of the two FastRPC grants is
expected in use; drop the other after the first permissive run.

`vendor_camera_sn_prop` labels `vendor.fp.camera_*`, the camera module serial numbers
the stock CamX tries to publish; nothing may set or read it. `vendor_init` may chmod the
stock 0777 `/mnt/vendor/persist/camera` and `cam_cali` directories.

Omitted: the TCL algorithm service and its data and dump directories, secure camera
(QSEECom/TEE, protected heaps, VM memory-buffer nodes, ssgtzd), the AON service, factory
OTP/OIS sysfs (`fp_mmitest_sysfs`), persist writes and QRTR ioctls. The file contexts restore
three upstream `generic/vendor/common/file_contexts` entries (lines 317, 348, 440). The
upstream files to reduce from are not imported, so there is no provenance entry.
These rules are not yet runtime-qualified under enforcing mode.

## Media

`media/` holds the policy for the stock Qualcomm Codec2 video service
(`vendor.qti.media.c2@1.0-service`, platform domain `mediacodec`), which exposes only the
hardware encoders (owner decision 2026-09-27; decoders stay the software codecs in the
sandboxed `mediaswcodec`). Every app except isolated processes is a Codec2 client and can
call it. The platform vendor policy already makes `mediacodec` the Codec2 HIDL server and a
gralloc and GPU client, and gives it read-write access to every `video_device` node. That
type also labels the camera V4L2, subdevice, media, JPEG and CVP nodes and the Iris decoder
node, so the DAC group is what keeps the codec off them: it runs in group `mediacodec`, and
`boot/ueventd.rc` gives that group only the encoder node `/dev/video33`; the decoder node
`/dev/video32` is root-only. The platform neverallows cover tcp, udp and rawip sockets; the
stock seccomp policies the service applies forbid `socket` and `connect` entirely. The
device grants add only what the encoder path uses: `SYS_NICE`, the read-only
`vendor.media.target_variant` (the service finds the target specification that limits it
to the encoders through it; without the read it would register every codec) and the
gralloc and Adreno properties. `vendor_init` may set `vendor.media.target_variant` from the
vendor build.prop. The file context restores upstream `generic/vendor/common/file_contexts`
line 324.

The composer's `IDisplayConfig` lookup (refresh rates for perf hints, which are off) is
denied and not audited: the service exposes screen writeback and panel controls. FastRPC is
denied the same way, with a neverallow on opening a DSP node, as in stock; `libfastcvopt`
then runs on the CPU.

Omitted: the Codec2 audio service, Wi-Fi display, VPP, hexlp, the capability config
store, secure video (content-protection and membuf nodes, QSEECom), FastRPC, the
performance HAL, `/data/vendor/media` and the Qualcomm system DMA-BUF heap (the selected
libraries allocate from the AOSP system heaps only). As for the camera, the upstream files
to reduce from are not imported, so there is no provenance entry. These rules are not yet
runtime-qualified under enforcing mode.
