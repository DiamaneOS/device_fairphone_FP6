# FP6 SELinux policy

Reference for the device policy in this directory: what each domain or file
may access, and why. It describes the policy of a release (`user`) build.

## Sources and layout

- Imported files are a selected subset of Fairphone's published Qualcomm
  policy (`device/qcom/sepolicy_vndr` and `device/qcom/sepolicy`).
  - `provenance.json` records each file's source path, revision and hashes.
  - Imported files keep their upstream headers.
- The selection covers the declared boot, display, credential, power, thermal,
  USB and basic hardware services.
- Not imported: manufacturing applications, diagnostics, unselected radio and
  camera services, and their test exceptions.
- A shared type declaration does not mean that the service is installed.
- This file explains the DiamaneOS-written rules. `dontaudit` rules inside
  imported files are not listed here.

| Directory | Policy |
|---|---|
| `vendor-common/`, `vendor-attributes/`, `qva-common/`, `vendor-volcano/`, `qva-volcano/` | imported Qualcomm vendor policy, with the adaptations below |
| `system-ext-public/`, `system-ext-private/`, `product-public/`, `product-private/` | imported Qualcomm system_ext and product policy, plus DiamaneOS rules |
| `fp6/` | device grants, each bound to one service |
| `audio/`, `bluetooth/`, `camera/`, `firmware/`, `gnss/`, `media/`, `modem/`, `nfc/`, `telephony/`, `timekeep/` | vendor policy of one subsystem each |
| `telephony-system-ext/`, `telephony-system-ext-public/` | system_ext side of the telephony apps |

## Rules for changes

- The policy is enforcing. Compatibility tests and neverallow checks stay on.
- Never fix a failure by importing stock policy wholesale or by adding a
  permissive domain.
- Bind hardware grants to the service domain, not to a HAL attribute. Keep the
  HAL IPC attributes a service needs.
- Adding a service means reviewing both its activation and its grants.
- Explain every `dontaudit` and every broad rule, here or next to the rule.
- Use platform init and ueventd permissions where they already cover an
  operation.
- Firmware-handler transitions are not imported. Review them if the product
  ever activates those handlers.

## Imported policy: adaptations

- Module loading stays in `vendor_modprobe`, the explicit init execution
  domain.
  - `vendor_init` has no module-loading capability. It only gets the directory
    traversal needed to restore the labels of the mounted persist partition.
  - `vendor_modprobe` has no `sys_nice` and no kernel task control. The camera
    module's request to raise its CCI IRQ thread priority stays denied
    (non-fatal).
- The Type-C class and sleep control use Android's genfs labels. The USB HAL
  gets class-directory enumeration and link traversal (`qva-common/hal_usb.te`).
- The USB speed node, which the USB HAL reads, is `sysfs_udc`. It is not the
  factory-test type `fp_mmitest_sysfs`, which also covers camera calibration
  and download mode.
- No HBTP access for the power HAL: the selected power service does not use
  it.
- No Qualcomm perf HAL domain or client grants (see [Power](#power)).
- The peripheral manager has no binder call into the WLAN service domain. That
  service is not installed.
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
  - Read-only SoC, thermal-zone and remote-processor names for the thermal,
    performance and peripheral-manager services; `/dev/wlan` and the driver
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
    domain off it. The UFS storage's serial number and its LUNs' SCSI serial
    and identification pages keep the generic `sysfs` label, readable by 22
    domains (among them the fingerprint and USB HALs, the composer and tee);
    ueventd makes them 0400 root too, and no shipped program reads them.
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
    is `vendor_proc_compaction` (`perf.te`). vendor_init writes all three; the
    perf HAL reads and writes only WALT and compaction (its SysNode 0xA).
    vendor_init also writes the platform `proc_sched` and
    `proc_watermark_scale_factor` sysctls; it still has no write on generic
    `proc`. The perf HAL keeps the imported `proc:file rw_file_perms`
    (`qva-common/hal_perf_default.te`), which should be narrowed.
  - camera.ko's CCI probe asks to raise the CCI IRQ thread from the kernel
    default SCHED_FIFO 50 to 99 from `vendor_modprobe`; this stays denied
    (non-fatal); `vendor_modprobe` has no `sys_nice` and no kernel task
    control.
  - Wi-Fi: `/proc/sys/net/ipv4/tcp_limit_output_bytes` is
    `vendor_proc_tcp_limit_output_bytes`, read and written only by the Wi-Fi
    HAL for high-throughput TCP tuning; the rest of `/proc/sys/net` stays
    read-only to it. `/data/vendor/tombstones` is no longer labelled as a
    whole (only `rfs/`), so the platform `tombstones/wifi` label for the HAL's
    ring-buffer logs applies. The factory MAC file the driver loads
    (`vendor_diamaneos_wlan_mac_file`, written by imeiprovd) is read only by
    ueventd, which serves it as firmware (`wlan_mac.te`; a neverallow keeps
    every other domain but init and vendor_init off it). Stock lets ueventd
    read all of `/mnt/vendor` and persist instead.
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
- `tee` (qseecomd), opening the GPT, XBL and boot block devices and the BSG
  nodes of the other UFS LUNs: it probes them at start; stock grants read on
  the block devices but never open, and labels none of those BSG nodes.
- `fp6_callaudio_app` and `fp6_iwlan_app`, `find` of `gpu_service` and
  `netstats_service` (and `content_capture` for the bridge): lookups every app
  process makes at start (ActivityThread's GraphicsEnvironment setup, for GPU
  driver statistics, and TrafficStats.init). Platform apps get them through
  `app_api_service`; these two domains have no graphics and keep no traffic
  statistics, and the framework continues when the lookup returns nothing
  (calls and Wi-Fi calling work with both denied).
- `hal_audio_default`, `read append` on `/sys/power/wake_lock`
  (`sysfs_wake_lock`), once per boot: PAL's `ResourceManager::initWakeLocks`
  opens the wake-lock nodes at start. Every PAL call of `acquireWakeLock` and
  `releaseWakeLock` is in sound trigger (`StreamSoundTrigger`,
  `SoundTriggerEngineGsl`; disassembly of the shipped `libar-pal.so`), which is
  not shipped. Playback with the screen off is kept awake by audioserver's own
  wake lock through the framework, and a call's audio runs between the modem
  and the audio DSP, so neither needs it. Stock grants it to the whole
  `hal_audio` attribute (`wakelock_use`).
- `platform_app` and `system_app`, the absent Google wireless-charger service.
- `platform_app` (SystemUI), `read` of `bluetooth_lea_prop`: SettingsLib's
  `BluetoothUtils.isAudioSharingSupported` reads
  `persist.bluetooth.leaudio_dynamic_switcher.mode`, which the platform lets
  only `bluetooth`, `system_app` and `gmscore_app` read. The property
  is not set on this build, so the denied read returns the same default.
  Platform app policy, as on other GrapheneOS devices; nothing added for apps.
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
- The touch controller's wake-gesture switch (`gesture_wakeup`) is
  `vendor_sysfs_touch_gesture` and its tap report (`wake_gesture`) is
  `vendor_sysfs_touch_wake_gesture`. The rest of the touch device keeps the
  - Only the sensors HAL uses them, for Tap to wake (double tap) and Tap to
    check phone (single tap): it writes the switch and reads the report
    (`fp6/touch.te`).
  - The power HAL has no access.
- `system-ext-private/system_server.te`: the system server reads enabled
  kernel-console metadata for the serial-console warning. It receives no
  access to serial devices or console control.

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

The primary HAL, PAL and AGM are built from source (`audio/provenance.json`)
and keep the stock domains and labels. The HAL registers the AGM HIDL service
(`IAGM`) in its own process and no PAL HIDL service (`IPAL` keeps its label).
`qva-common/hwservice_contexts` labels both `vendor_hal_audio_internal_hwservice`
(`audio/hwservice.te`) instead of `hal_audio_hwservice`, so only
`hal_audio_default` may register and look them up; audioserver, system_server
and the Bluetooth stack, which are hal_audio clients, cannot reach them.
The kernel may not search `/mnt/vendor` (a platform neverallow) and does not
need to: the amplifier driver leaves calibration to userspace. At each speaker
start the audio HAL's PAL reads the per-unit calibration from persist and writes
it to the DSP (logged as "Awinic set cali re success"; checked on the phone, as
on stock). These rules are not yet runtime-qualified under enforcing mode;
collect denials on the device before adding any grant.

## Modem

- The stock peripheral manager boots the modem (MSS) through its remoteproc
  character device, which ueventd makes 0600 for `system`.
- The modem's `study` partition has the stock
  `vendor_modem_efs_partition_device` label (`vendor-volcano/file_contexts`),
  so `rmt_storage` serves it with its existing grant.
- The full RAM dump collector (`vendor_subsystem_ramdump`) is not installed
  and has no policy.
- Remote-processor error logs are not written to `/data`: there is no
  `/data/vendor/tombstones/rfs`, and tqftpserv serves no ramdump paths.

### `vendor_ssr_setup`

The stock subsystem-restart helper (`modem/ssr_setup.te`), reduced from
Qualcomm's `ssr_setup.te` (`provenance.json`).

- It lists `/sys/class/remoteproc` and reads the processor names.
- It writes only the `recovery` switches (`vendor_sysfs_ssr_toggle`).
- It reads `persist.vendor.ssr.*`.
- Not granted, unlike upstream: writes to `vendor_sysfs_ssr` files and links
  (the legacy restart-level interface, absent on this kernel) and reads of
  `vendor_sysfs_data`.
- `modem/file_contexts` labels `/vendor/bin/ssr_setup`. The MSS, ADSP, CDSP
  and WPSS recovery switches are in `vendor-volcano/file_contexts`.

### tqftpserv and pd-mapper

- The modem's TFTP file server and the protection-domain mapper are the
  open-source linux-msm `tqftpserv` (DiamaneOS fork) and `pd-mapper`, built
  from `vendor/qcom/opensource`.
- They replace Qualcomm's `tftp_server` and `pd-mapper`.
  - `rfs_access.te`, `pd_services.te` and their file contexts are not
    imported.
  - There are no `/vendor/rfs` links into persist.
- Each runs as its own vendor user (`modem/config.fs`) with no capabilities,
  no network, no binder and no properties.
- Both read the modem partition, which is mounted for `system`
  (`fstab.qcom`), through the `system` supplementary group. SELinux limits
  what they can read.
- QRTR has no per-service access control, so either service can reach any QMI
  service. `telephony/qrtr.te` lists both in its inventory.

`vendor_tqftpserv` (`modem/tqftpserv.te`):

- QRTR sockets: create, connect, getattr, read, setopt and write. No ioctl or
  bind.
- It lists `/sys/class/remoteproc` and reads each processor's `firmware`
  attribute (`vendor_sysfs_rproc_firmware`, `modem/file_contexts`).
- Read-only requests (`modem_pr/...`) are served through the
  `/vendor/firmware/modem_pr` link to the modem partition.
- Its read-write directory is `/data/vendor/tmp/tqftpserv`
  (`vendor_tqftpserv_data_file`).
  - It may open, create, read, write and delete files there. No rename, link,
    attribute change or subdirectory.
  - The modem writes `server_check.txt` and `mcfg.tmp` there, and deletes
    `mcfg.tmp` (the TFTP "unlink" option).
  - tqftpserv deletes only files in that directory and follows no symbolic
    links there.
  - Nothing goes to persist, so a factory reset clears these files.
- `dontaudit` on reading the kernel's firmware search path
  (`vendor_sysfs_firmware_path`, `modem/genfs_contexts`): tqftpserv probes it
  on every request.
  - On the FP6 that path is a comma-separated list it cannot use, so the read
    stays denied and it uses `/vendor/firmware`.
- A neverallow keeps other vendor services from changing its files.

`vendor_pd_mapper` (`modem/pd_mapper.te`):

- QRTR sockets: create, getattr, read, setopt and write.
- The same remoteproc listing and `firmware` attribute.
- Its domain lists (`/vendor/firmware/*.jsn`, links to the modem partition)
  are readable through the platform's vendor-file rules.
- Not granted, unlike upstream: `vendor_sysfs_data`,
  `persist.vendor.pd_locater_debug`, and the legacy IPC router socket with its
  ioctl.

## Bluetooth

- DiamaneOS's HCI service (`bluetooth/service.cpp`) hosts the stock Qualcomm
  HCI implementation. `bluetooth/file_contexts` gives it the label of the
  stock service it replaces.
- It reads the chip's compatible string from btpower, sets the SoC name and
  registers the stock HCI implementation.
- `bluetooth/hal_bluetooth_default.te` is written by DiamaneOS from the rules
  the stock compiled policy has for that service. It has no provenance entry.

`hal_bluetooth_default`:

- Uses the btpower node.
- Reads the Bluetooth firmware partition and the Bluetooth persist directory.
- Reads the Bluetooth vendor properties and the SoC identification.
- Sets only the properties it writes at run time: the SoC name,
  `scram.enabled`, a generated address and two crash counters.
  - `bluetooth/property_contexts` gives them the vendor-internal type
    `vendor_bluetooth_hal_state_prop`, so no platform domain or app can read
    the address.
- Gets the HCI UART (`hci_attach_dev`) from platform policy, as in stock.

Factory address:

- `ro.vendor.bt.boot.macaddr` has its own vendor-internal type,
  `vendor_bluetooth_address_prop`. The HAL reads it before the generated
  address, and may only read it.
- Only vendor_init sets it (`bluetooth/vendor_init.te`):
  `init.fp6.bluetooth.rc` copies the value imeiprovd read from the
  traceability partition, as stock does.
- Neverallows: only init and vendor_init set it. Only they, dumpstate and the
  HAL read it.

Not granted:

- Unlike the stock policy: persist writes, QRTR sockets (used only for the
  modem-NV address query, which is off), `/data/vendor/bluetooth`, diag, the
  FM radio device, the ssgtzd socket, TPI and Xpan service registration and
  HSUART tracing.
- The FM, ANT, SAR, config-store and TPI libraries are not installed.
- `hwservice_contexts` has no lines for the ANT, ANT HCI and Bluetooth SAR
  interfaces, so the HAL cannot register them.
  - Unmapped names fall to `default_android_hwservice`, which no domain may
    add.

Shared device labels (`vendor-common/file_contexts`):

- `/dev/btfmcodec_dev` and `/dev/bt_cp_ctrl` share `hci_attach_dev` with
  `/dev/ttyHS0`.
- `/dev/btfmslim` shares `vendor_bt_device` with `/dev/btpower`.
- SELinux alone would let the HAL use them. ueventd keeps the three root-only.

## NFC

- `nfc/file_contexts` labels the stock Samsung NFC HAL
  (`android.hardware.nfc-service.sec`) `hal_nfc_default_exec` and the
  controller node `/dev/sec-nfc` `nfc_device`: the stock labels.
- The domain `hal_nfc_default` and its controller access come from AOSP
  policy. No device rule is added.
- The Qualcomm `hal_nfc_default.te`, `nfc.te`, `nqnfcinfo.te`,
  `hal_secure_element_default.te` and `secure_element.te` are not imported.
  - Most of their grants are for NXP/QTI parts that are not installed or not
    used by the Samsung HAL.
  - Examples: HIDL registration (the HAL registers only AIDL `INfc/default`),
    the NXP NFC service, `vendor.qti.nfc.*` properties, the `ssgtzd` socket,
    `/firmware` and `/data/vendor/nfc`.
- The controller firmware in `/vendor/firmware` needs no grant: vendor domains
  may read `vendor_file_type` through platform policy.
- Denied on purpose: the HAL's set of `vendor.nfc.fw.version`
  (`vendor_nfc_prop`). Its library ignores the result and never reads the
  property back.
- Denied on purpose: the HAL's read of `persist.sys.factory.mode`
  (`system_prop`). That keeps it on the normal firmware path.
- `/data/nfc`, which platform init.rc creates for the NFC stack's state, is
  `nfc_data_file` (`system-ext-private/file_contexts`, as in stock).

## GNSS

- `gnss/hal_gnss_qti.te` confines the Qualcomm GNSS HAL
  (`android.hardware.gnss-aidl-service-qti`), built from CodeLinaro source, in
  `vendor_hal_gnss_qti`.
- It is written by DiamaneOS from the stock compiled policy, reduced to what
  the HAL uses. It has no provenance entry.
- The service is a HAL server for `hal_gnss` and a client of `hal_health`.
- It reaches the modem over QRTR (`qipcrtr_socket`, no ioctls). The grant is
  bound to the domain, not to the `hal_gnss` attribute.
- It keeps small state files in `/data/vendor/location`.
- It reads the modem version file from `/vendor/firmware_mnt`, the boot status
  property, and the public SoC id and board type.
  - `gnss/genfs_contexts` labels `/sys/devices/soc0/hw_platform`
    `vendor_sysfs_public`, so the HAL needs no read of the rest of
    `vendor_sysfs_soc`, which includes the SoC serial number.
- No servicemanager rule is added: platform policy covers AIDL HAL
  registration, as for the gatekeeper and keymint HALs.
- The HAL opens no socket in `/dev/socket/location`.
- Not granted, because none of these peers is installed:
  - the peripheral manager and vndbinder
  - the location daemons' sockets (loc_launcher, XTRA, LOWI, Wi-Fi
    crowdsourcing, sensor, correction, engine and Skyhook services)
  - RIL and SSG sockets, MHI sysfs and QMS/AON properties.

    SUSTAINED_PERFORMANCE or EXPENSIVE_RENDERING, and ADPF hint sessions.
    DOUBLE_TAP_TO_WAKE does nothing: double tap goes through the sensors HAL.
    `sched_boost` values, the GPU wake trigger and uclamp on session threads.
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
are the stock labels. `telephony/service.te`, `qcril_audio.te`, `qti_lpa.te`, the two
are the stock labels. `telephony/service.te`, `fp6_callaudio_app.te`, the
`fp6_*_app` domains and the relabelling of IQcRilAudio and IUimLpa in
`vendor-common/service_contexts` are downstream.

The stock radio daemon runs in the platform `rild` domain. It may add only the four
Qualcomm radio services the device declares (IMS and radio config under
`vendor_hal_telephony_service2`, audio messenger and LPA under their own types). It is not
a `binderservicedomain`; each selected app client has an explicit binder grant. The
secure-element HAL, unrelated data-factory services, diag, the legacy
IPC-router ioctls, the QCRIL client socket and executing vendor tools are not granted.
The radio daemon and nicmd may use TIPC sockets with each other, as on stock: the data
module's DSI layer waits for nicmd over TIPC before it allows any data call. No other
domain may create a TIPC socket (neverallow in `telephony/rild.te`), and the kernel builds
TIPC for local IPC with network bearer creation blocked, without UDP, crypto or diag modules.
nicmd keeps its netlink, QRTR, rmnet ioctl and network-wrapper access, datagram sockets
for interface ioctls and its init-created recovery file. Its XFRM netlink access is
needed for Wi-Fi calling, where the modem negotiates the ePDG tunnel and nicmd installs
the ESP states and policies it receives (write requests); to remove them at teardown or
rekey it dumps all states and deletes those whose protocol and SPI match, the only read
request it sends (`telephony/nicmd.te`). That dump covers every IPsec state on the
device, with the keys zeroed by lockdown and the kernel's own XFRM redaction. It may `node_bind` TCP and UDP
sockets, to reserve the ephemeral ports the modem's embedded clients use, and read the
public SoC id for data target detection. It is not a `netdomain` (no TCP connect, no
`name_bind`); its remote-processor probe (`vendor_sysfs_ssr`) stays denied because the
result is unused on this SoC. The SHS, QMI-priority and performance helpers are not
installed and not granted.

The stock IMS app, QCRIL audio messenger and eSIM LPA keep their stock Fairphone
signature. They are selected through a dedicated seinfo (`fp6_stock_platform`,
`telephony-system-ext/mac_permissions.xml`), their package or process name and
`isPrivApp=true`, never through our platform key. Each has its own domain: the IMS app
(`vendor_qtelephony`) may call the radio daemon and find the IMS and radio-config
services; the audio messenger (`fp6_qcril_audio_app`) only IQcRilAudio and the audio
APIs; the LPA (`fp6_qti_lpa_app`) only IUimLpa, the telephony and connectivity APIs and
The stock IMS and IWLAN/certificate apps retain Fairphone's signature and a
signer-specific seinfo, with privileged placement and package/process selectors.
The IMS app runs in `vendor_qtelephony`; the coupled IWLAN/certificate frontend
runs in `fp6_iwlan_app` with explicit radio Binder and certificate QRTR access.
Package selectors label its data separately from the shared-process selector.
The inactive LPA, its download grants and app-domain policy are omitted; the
native IUimLpa interface remains because the selected radio binary depends on it.
These presigned apps retain OEM update trust; release signing and update policy
remain separate trust decisions. Collect denials before adding any grant.
QtiTelephonyService. Its signer, package name and privileged placement select the domain, and it runs in
- `dontaudit` for its link dump (`netlink_route_socket` `nlmsg_readpriv`) and
  raw ICMPv6 socket (`rawip_socket` `create`) during Wi-Fi calling setup.
  - Stock denies both too, and calls and handover work without them.
  - The link dump would show every interface's MAC address.
provider's own vndbinder open, `/data/vendor/camera`, read-only factory calibration in
service. For streaming the provider is a client of the graphics allocator (buffer
allocation and IMapper) through `hal_client_domain`, the only form the platform
neverallows allow for the allocator service lookups. The membership also lets it find the
mapper services, execute `same_process_hal_file` (the passthrough IMapper), call
servicemanager and share memfds with the allocator, and, as a `halclientdomain`, call
hwservicemanager, read `hwservicemanager_prop` and find `hidl_manager_hwservice`. For CamX
perf locks it may only make binder calls into the perf HAL (`vendor_hal_perf_default`):
a permissive boot logged no IPerf service lookup, so it is not a perf HAL client; if a find
denial on `vendor_hal_perf2_service` or `vendor_hal_perf_hwservice` shows up, make it one
with `hal_client_domain`. It may use the composer's release fences, read the public SoC id and
search `/sys/devices/soc0` for the per-part files (`num_subset_parts` is labelled in
`camera/genfs_contexts`). Denied: the display QService and display-config lookups
(IDisplayConfig would expose brightness, power mode and writeback capture),
IPostProcService registration and the property-area listing CamX does at start (silent
under enforcing). Unlike the sensors HAL, the camera may need the non-secure FastRPC node:
the kernel runs the CDSP as a non-secure channel. Only one of the two FastRPC grants is
