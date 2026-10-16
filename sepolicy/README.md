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
    domain off it. The UFS storage's serial number and its LUNs' SCSI serial
    and identification pages keep the generic `sysfs` label, readable by 22
    domains (among them the fingerprint and USB HALs, the composer and tee);
    ueventd makes them 0400 root too, and no shipped program reads them.
  - Thermal HAL: runs as system with no capabilities
    (`power/init.fp6.thermal.rc`); ueventd gives group system the two trip
    nodes it writes (`trip_point_1_temp` and `trip_point_1_hyst` of each
    thermal zone). Its domain is unchanged.
  - Thermal engine (closed, root): on userdebug and eng builds,
    `fp6/thermal_engine_audit.te` logs each node it opens for writing, the
    device nodes it opens, the sockets it creates, its capability use and its
    shutdown requests ("avc: granted" records). These rules grant nothing;
    user builds do not have them.
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
- Only `qseecomd`, KeyMint, Gatekeeper and the fingerprint HAL may open,
  read, write or send commands to the node. A neverallow keeps every other
  domain off it (`tee_node.te`).
- The four may send two commands: invoke (`0x9801`) and create a callback
- The touch controller's wake-gesture switch (`gesture_wakeup`) is
  `vendor_sysfs_touch_gesture` and its tap report (`wake_gesture`) is
  `vendor_sysfs_touch_wake_gesture`. The rest of the touch device keeps the
  - Only the sensors HAL uses them, for Tap to wake (double tap) and Tap to
    check phone (single tap): it writes the switch and reads the report
    (`fp6/touch.te`).
  - The power HAL has no access.
- `qseecomd` on `/mnt/vendor/persist/drm`: no keybox or HDCP provisioning
  tool is installed to fill that store.
- `system-ext-private/system_server.te`: the system server reads enabled
  kernel-console metadata for the serial-console warning. It receives no
  access to serial devices or console control.
- `system-ext-private/camera_table.te`: the camera provider may read
  `ro.vendor.qti.va_aosp.support`, which selects the stock camera table with
  every lens (`camera/camera.mk`).
  - The rule names the `hal_camera_server` attribute, because the property's
    type is private to this partition. The provider is its only member here.

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

## Power

- The power HAL is LineageOS's libperfmgr
  (`android.hardware.power-service.lineage-libperfmgr`, device `power/`) in
  the platform `hal_power_default` domain (`fp6/power.te`).
- Qualcomm's perf daemon is not installed. Not imported: its domain
  (`hal_perf_default.te`), its file and service contexts, and the perf client
  grants of the composer, SurfaceFlinger, the camera and the power HAL.
  - Kept as declarations only: Qualcomm's HIDL perf hwservice types and
    contexts and the `vendor_hal_perf` attributes.
- Process: user and group system, CAP_SYS_NICE only (`power/init.fp6.power.rc`
  overrides the module's root service).
  - No readproc group: the HAL reads no `/proc/<pid>` entries.
- Nodes the HAL may write, not read:
  - `scaling_min_freq` and `scaling_max_freq` of the CPU policies
    (`vendor_sysfs_cpufreq_limit`, `fp6/genfs_contexts`)
  - the GPU devfreq `min_freq` and `max_freq` and the GPU wake trigger
    `touch_wake` (`vendor_sysfs_kgsl_limit`, `fp6/file_contexts`)
  - the touch gesture switch (`fp6/touch.te`).
- init hands exactly these nodes to system. The rest of the CPU and GPU sysfs
  stays root's, with its platform or Qualcomm label.
- Thermal engine: SELinux keeps its read and write on the relabelled nodes.
  - The CPU and GPU frequency nodes belong to system (0644) and the root
    engine has no `dac_override`, so it cannot write them.
- system_server keeps read on the CPU limit nodes, as under the platform
  label.
- `sched_boost`: `/proc/sys/walt/sched_boost` is root-only, and a sysctl
  cannot be chowned.
  - The HAL may set only `vendor.powerhal.sched_boost`
    (`vendor_power_sched_boost_prop`, values 0, 1 and 2). Vendor init writes
    the matching fixed value.
  - The other `vendor.powerhal.*` properties (`vendor_power_prop`) are
    switches the HAL reads and vendor init sets.
  - `vendor.powerhal.sched_boost.enable=false` turns the `sched_boost` part of
    the hints off.
- ADPF: `setsched` on apps, SurfaceFlinger and system_server, with
  CAP_SYS_NICE, to set uclamp on hint-session threads.
  - The domain is an `mlstrustedsubject`: the platform MLS constraint on
    setsched requires equal levels, and apps run with categories.
  - setsched is its only access to app processes.
  - It is a thermal HAL client, for the throttling state.
- `libqti-perfd-client` (`power/libqti-perfd-client`) is a source stand-in for
  the closed client library that the stock camera and SDM extension load by
  name.
  - It forwards only the camera's open, close and snapshot hints, as
    time-limited CAMERA_LAUNCH and CAMERA_SHOT boosts. Every other call does
    nothing.
  - It is `vendor_file`: no app process loads it.
- Clients: the platform's (system_server, SurfaceFlinger, apps' hint sessions
  through system_server) and one vendor client, the camera provider
  (`camera/hal_camera_default.te`).
  - A client reaches every IPower method: boosts, modes such as
    SUSTAINED_PERFORMANCE or EXPENSIVE_RENDERING, and ADPF hint sessions.
    DOUBLE_TAP_TO_WAKE does nothing: double tap goes through the sensors HAL.
  - Their effects stay within the grants above: frequency floors and caps, the
    `sched_boost` values, the GPU wake trigger and uclamp on session threads.
- Not granted:
  - reads of the nodes (dumpsys shows request indexes, not values)
  - the debug configuration in `/data/vendor/etc`
    (`vendor.powerhal.config.debug`)
  - the Pixel-only `/proc/vendor_sched`
  - the display `idle_state` nodes: the Qualcomm display driver has none
    (`vendor.powerhal.disp.idle_support=false`)
  - setsched on the composer, which is needed only if SurfaceFlinger puts
    composer threads in its hint session.

## Power stats

- The power stats HAL (`android.hardware.power.stats-service.fp6`, device
  `power/stats`) runs in the platform `hal_power_stats_default` domain
  (`fp6/power_stats.te`).
- Its clients are the platform's IPowerStats clients.
- Process: its own vendor user and group `vendor_powerstats`
  (`power/config.fs`), no supplementary groups, no capabilities.
- Device: `/dev/stats`, the qcom_stats driver's sleep counter node, is
  `vendor_qcom_stats_device` (`fp6/file_contexts`) and 0400 for the HAL's user
  (`boot/ueventd.rc`).
  - The HAL may open, read and ioctl it, nothing else.
  - A neverallow keeps other vendor domains off it, and the HAL from writing
    it.
- ioctls: an `allowxperm` limits the HAL to the commands it uses: modem,
  WPSS, ADSP and CDSP sleep, and the AOSD, CXSD and DDR records.
  - The driver's other commands are not allowed.
- Not used: debugfs, sysfs, properties and energy meters. The FP6 has no
  on-device power monitor.

## Telephony

Sources:

- `telephony/rild.te`, `nicmd.te` and `qtelephony.te` are reduced from
  Qualcomm's vendor policy. `telephony-system-ext/vendor_qtelephony.te` and
  `seapp_contexts` from Qualcomm's system policy.
  - Each file's header names its upstream files and revision.
- The file contexts are the stock labels.
- DiamaneOS's own: `telephony/service.te`, the `fp6_*_app` domains, and the
  relabelling of IQcRilAudio and IUimLpa in `vendor-common/service_contexts`.

Radio daemon (`telephony/rild.te`):

- The stock radio daemon runs in the platform `rild` domain.
- It may add only the Qualcomm radio services the device declares: IMS and
  radio config under `vendor_hal_telephony_service2`, call audio and LPA
  under their own types.
- It is not a `binderservicedomain`. Each selected app client has an explicit
  binder grant.
- Not granted: the secure-element HAL, unrelated data-factory services, diag,
  the legacy IPC-router ioctls, the QCRIL client socket and executing vendor
  tools.
- The radio daemon and nicmd may use TIPC sockets with each other, as on
  stock: the data module's DSI layer waits for nicmd over TIPC before it
  allows any data call.
  - No other domain may create a TIPC socket (neverallow in `rild.te`).
  - The kernel builds TIPC for local IPC with network bearer creation blocked,
    without UDP, crypto or diag modules.

nicmd (`telephony/nicmd.te`):

- It keeps its netlink, QRTR, rmnet ioctl and network-wrapper access,
  datagram sockets for interface ioctls and its init-created recovery file.
- XFRM netlink access is needed for Wi-Fi calling: the modem negotiates the
  ePDG tunnel, and nicmd installs the ESP states and policies it receives
  (write requests).
  - To remove them at teardown or rekey, it dumps all states and deletes those
    whose protocol and SPI match. That dump is the only read request it sends.
  - The dump covers every IPsec state on the device, with the keys zeroed by
    lockdown and the kernel's own XFRM redaction.
- It may `node_bind` TCP and UDP sockets, to reserve the ephemeral ports the
  modem's embedded clients use.
- It reads the public SoC id, for data target detection.
- It is not a `netdomain`: no TCP connect and no `name_bind`.
- Capabilities: `net_admin` and `net_raw`. A neverallow forbids setuid,
  setgid, setpcap and kill.
- Its remote-processor probe (`vendor_sysfs_ssr`) stays denied: the result is
  unused on this SoC.
- `dontaudit vendor_nicmd kernel:system module_request`: kernel module
  requests that nicmd triggers stay denied without an audit record. Nothing
  grants nicmd module loading.
- `dontaudit` for its link dump (`netlink_route_socket` `nlmsg_readpriv`) and
  raw ICMPv6 socket (`rawip_socket` `create`) during Wi-Fi calling setup.
  - Stock denies both too, and calls and handover work without them.
  - The link dump would show every interface's MAC address.
- The SHS, QMI-priority and performance helpers are not installed and not
  granted.

Apps:

- The stock IMS and IWLAN/certificate apps keep Fairphone's signature and a
  signer-specific seinfo, with privileged placement and package or process
  selectors.
- The IMS app runs in `vendor_qtelephony`.
- The coupled IWLAN/certificate frontend runs in `fp6_iwlan_app`, with
  explicit radio Binder and certificate QRTR access.
  - Package selectors label its data separately from the shared-process
    selector.
- No policy for the stock LPA app: no app domain and no download grants. The
  native IUimLpa interface remains, because the selected radio binary depends
  on it.
- These presigned apps retain OEM update trust: a same-signature update of one
  of them still matches its selector.
- DiamaneOS's call-audio bridge (`de.diamaneos.callaudio`, `callaudio/`)
  replaces the stock QtiTelephonyService and runs in `fp6_callaudio_app`.
  - Its signer, package name and privileged placement select the domain.
  - It may find only IQcRilAudio, the audio server and the activity manager,
    and make binder calls with the radio daemon.
  - Neverallow: no network or QRTR sockets.

QRTR inventory (`telephony/qrtr.te`):

- A neverallow names every domain that may create a QRTR socket.
- QRTR has no per-service access control, so adding a domain needs a
  trust-boundary review.

## Camera

`camera/` holds the policy for the stock CamX/CHI provider
(`vendor.qti.camera.provider-service_64`, platform domain
`hal_camera_default`).

- The grants are written from the stock compiled vendor policy and bound to
  `hal_camera_default`, not to the `hal_camera` attributes. There is no
  provenance entry.

Grants:

- `sys_nice` and wake locks.
- The ToF node and UBWC-P.
- The camera sub-device nodes (`/dev/v4l-subdev*`: sensors, actuators, OIS,
  flash), under their own type `vendor_camera_subdev_device`.
  - Only the provider may open them.
  - They do not share `video_device`, which the composer and platform media
    services also hold.
- FastRPC with read-only opens: the secure node only (the ADSP sensors PD of
  the CamX sensor direct channel and the CDSP offloads) and `/vendor/dsp`.
- Qualcomm DMA-BUF heaps. On the display heap only the allocation ioctl.
- SoC, camera, JPEG and DDR identification, `/proc/meminfo` and the public SoC
  id.
  - It may search `/sys/devices/soc0` for the per-part files
    (`num_subset_parts` is labelled in `camera/genfs_contexts`).
- QRTR sockets without ioctls, for the gyro QMI client.
- The thermal-engine client socket.
- The provider's own vndbinder open. No vndservice lookup.
- `/data/vendor/camera`, read-only factory calibration in
  `/mnt/vendor/persist/camera`, and the vendor camera properties.
- The composer's release fences.
- Graphics allocator client, for buffer allocation and IMapper.
  `hal_client_domain` is the only form the platform neverallows allow for the
  allocator service lookups.
  - The membership also lets it find the mapper services, execute
    `same_process_hal_file` (the passthrough IMapper), call servicemanager and
    share memfds with the allocator.
  - As a `halclientdomain` it may call hwservicemanager, read
    `hwservicemanager_prop` and find `hidl_manager_hwservice`.
- Power HAL client, as on Pixels: CamX's perf hints go to
  `libqti-perfd-client` (device `power/`), which forwards the open, close and
  snapshot hints as time-limited boosts.
  - `hal_client_domain(hal_camera_default, hal_power)` is the only form the
    platform neverallow on `hal_power_service` lookups allows.
  - It lets the provider find the power service, make binder calls with the
    power HAL both ways (the HAL never calls back) and share memfds with it.
  - It also lets it find the HIDL power service, which nothing registers.
  - SELinux cannot limit the grant to the camera boosts: it covers the whole
    IPower interface (see [Power](#power)).
- `crash_dump_fallback`: the provider runs with no_new_privs under its seccomp
  filter (`camera/seccomp`), so debuggerd writes its tombstones and stack
  dumps through the in-process fallback handler.

Denied on purpose:

- The display QService and display-config lookups: IDisplayConfig would expose
  brightness, power mode and writeback capture.
- IPostProcService registration.
- `dontaudit hal_camera_default default_prop:file read`: the property-area
  listing CamX does at start.
- `dontaudit` on adding the offline camera service: it is in no VINTF
  manifest and nothing uses it. The provider retries at every start and
  carries on.
- `dontaudit` on looking up the ODM's camera algorithm service and Qualcomm's
  post-processing HAL: neither is in the image, and the provider asks at every
  session.
- `vendor_camera_sn_prop` labels `vendor.fp.camera_*`, the camera module
  serial numbers CamX tries to publish. Nothing may set or read it, and the
  denials stay audited.

Other:

- `vendor_init` may create the camera data directory, set the camera
  properties and change the mode of the persist `camera` and `cam_cali`
  directories.
- Not granted, unlike the stock policy: the TCL algorithm service and its data
  and dump directories, secure camera (QSEECom/TEE, protected heaps, VM
  memory-buffer nodes, ssgtzd), the AON service, factory OTP/OIS sysfs
  (`fp_mmitest_sysfs`), persist writes and QRTR ioctls.

## Media

`media/` holds the policy for the stock Qualcomm Codec2 video service
(`vendor.qti.media.c2@1.0-service`, platform domain `mediacodec`).

- It exposes the hardware encoders, and the hardware decoders the user's
  Hardware video decoding choice allows (`media/media.mk`):
  - never, the default: none.
  - all video: the three clear decoders. No secure decoder is offered.
- Every app except isolated processes is a Codec2 client and can call it.
- The platform vendor policy makes `mediacodec` the Codec2 HIDL server and a
  gralloc and GPU client, and gives it every `video_device` node.
  - That type also labels the camera V4L2, subdevice, media, JPEG and CVP
    nodes, so the DAC group (`mediacodec`) is what keeps the codec off them.
- Codec nodes: `/dev/video32` (decoder) and `/dev/video33` (encoder) have
  their own type, `vendor_video_codec_device`.
  - Only `mediacodec` may open them (neverallow), with the V4L2 ioctls only
    (`video_ioctl2` is the driver's only handler).
  - The decoder node is root-only when the choice is never.
  - Otherwise `vendor_init` changes its group and mode at early-boot, through
    the platform's setattr grant on every device node, and may `getattr` it.
- `persist.diamaneos.hw_video_decode`: the user's choice. Accepts only 0
  (never) and 1 (all video). Unset counts as 0.
  - Only Settings sets it (`system-ext-private/hw_video_decode.te`).
  - It is public and system restricted, because the vendor rc triggers on it.
- `ro.vendor.diamaneos.hw_video_decode`: the state of the running boot.
  Accepts only 0 and 1.
  - Only init and `vendor_init` set it. The codec service and Settings read
    it, and the vendor rc file triggers on it.
- No network: the platform neverallows cover tcp, udp and rawip sockets.
  - DiamaneOS's seccomp filter (`media/seccomp`) fails every socket family but
    AF_UNIX with EPERM, and the stock one has no `socket` at all.
- Other grants: `SYS_NICE`, the gralloc and Adreno properties, and
  `vendor.media.target_variant`.
  - Through that property the service finds the target specification that
    limits the codecs it registers.
  - `vendor_init` sets it from the vendor build.prop and switches it when a
    hardware decoder is offered.
- `dontaudit` on the lookup of the composer's `IDisplayConfig` (refresh rates
  for perf hints, which are off): the service exposes screen writeback and
  panel controls, so the lookup stays denied.
- `dontaudit` on the FastRPC nodes, with a neverallow on opening a DSP node,
  as upstream: `libfastcvopt` then runs on the CPU.
- Not granted, unlike the stock policy:
  - the Codec2 audio service, Wi-Fi display, VPP, hexlp and the capability
    config store
  - the performance HAL and `/data/vendor/media`
  - the Qualcomm system DMA-BUF heap: the selected libraries allocate from the
    AOSP system heaps only.
- Secure video is not granted either: the memory-buffer driver and the
  content-protection bitstream node.
- There is no provenance entry for these files.

### DRM

- ClearKey (`hal_drm_clearkey_aidl`) runs under the platform policy alone. No
  device rule.
  - Its service runs without the `drmrpc` group (`media/init.fp6.drm.rc`), and
    the TEE node does not carry the label the platform grants DRM HALs
    (see "TEE node").
- Widevine (`hal_drm_widevine_sw`, `media/hal_drm_widevine_sw.te`) is the
  stock service from Fairphone's package with its built-in software
  protection only. The hardware-backed library is not installed.
  - Granted: the DRM HAL interface and its data in `/data/vendor/mediadrm`.
    The rest is what the platform gives every DRM HAL.
  - No rule lets it use the TEE node, the QSEECom heap, the crypto engine
    node, the socket to `qseecomd`, the memory-buffer driver, the bitstream
    node or the codec service's file descriptors.
  - Its service runs without the `drmrpc` and `system` groups
    (`media/init.fp6.drm.rc`).

## Recovery

- The recovery executable uses the boot-control service over IPC. It does not
  link the legacy Qualcomm updater.
- So this policy gives recovery no raw SCSI/BSG access and no CAP_SYS_RAWIO.
- Boot control keeps BSG access and CAP_SYS_RAWIO for the UFS implementation.
- Legacy sg character-device access is not granted. The sysfs discovery used
  by `gpt-utils` remains.
- Read and write access to recovery UI and firmware resources belongs to the
  init, ueventd or recovery domain that performs the operation.

## Not granted on purpose

These accesses stay denied. Each was checked against what the process does
without it. Denials covered in the sections above are not repeated.

- `system_server`, extcon cable names of the EUD debug port (`sysfs`): no
  cable type the framework uses.
- `system_server` (InputReader), `max_brightness` of the haptics LED device
  (`sysfs_leds`): reading it would let InputReader treat the vibrator as a
  light.
- `hal_camera_default`, `ro.vendor.qti.soc_id` (`vendor_soc_id_prop`): not
  set on this build. CamX uses the public SoC id file.
- `hal_bluetooth_default` and `vendor_hal_gnss_qti`,
  `ro.vendor.qti.va_aosp.support`: the property is set for the camera provider
  alone (`system-ext-private/camera_table.te`). A denied read returns the
  default these two had before it was set.
- `hal_bluetooth_default`, `hal_camera_default` and `vendor_hal_gnss_qti`,
  `ro.vendor.qti.va_odm.support`: not set on this build, so a denied read
  returns the same default.
- `hal_audio_default` and `rild`, `persist.vendor.pd_locater_debug`
  (`vendor_pd_locater_dbg_prop`): a debug switch, off when unreadable. No
  domain reads it here.
- `vendor_nicmd`, `property_socket` write and an `init.svc.*` read: the SHS
  and QMI-priority helpers it would start and check are not installed.
- `fp6_iwlan_app` and `mediacodec`, reads of `default_prop` and
  `zygote_config_prop`: generic platform properties that a denied read
  returns as unset. Neither needs them.
- `hal_camera_default`, finding
  `vendor.tcl.camera.algoservice.ITctCameraAlgoService`: the TCL algorithm
  service is not installed, and CamX continues without it.
- `tee` (qseecomd), opening the GPT, XBL and boot block devices and the BSG
  nodes of the other UFS LUNs: it only probes them at start.
- `fp6_callaudio_app` and `fp6_iwlan_app`, `find` of `gpu_service`,
  `netstats_service` and `content_capture_service`: lookups every app
  process makes at start.
  - These two domains have no graphics and keep no traffic statistics, and the
    framework continues when the lookup returns nothing.
- `hal_audio_default`, `read append` on `/sys/power/wake_lock`
  (`sysfs_wake_lock`): PAL opens the wake-lock nodes at start, but only sound
  trigger, which is not shipped, takes wake locks.
  - audioserver's own wake lock keeps playback awake with the screen off. A
    call's audio runs between the modem and the audio DSP.
- `platform_app` and `system_app`, the absent Google wireless-charger service.
- `platform_app` (SystemUI), `read` of `bluetooth_lea_prop`: the property is
  not set on this build, so the denied read returns the same default.
  - This is platform app policy, as on other GrapheneOS devices.
- `untrusted_app`, probes of the device (adb properties, `/proc`, `/sys`,
  `selinuxfs`, the root and `/dev` directories, the wallet property, cgroups):
  intended.

## Updating upstream policy

The source of truth is the Fairphone-published Qualcomm repositories at the
revisions in `provenance.json`. The local files are a reviewed subset with
the adaptations above.

- Do not track a moving branch in the build.
- Do not include the entire upstream `SEPolicy.mk`: it would activate policy
  for services the product does not install, and could undo domain scoping.

For each upstream refresh:

1. Fetch the two declared repositories into review checkouts, keeping the old
   revisions available.
2. Verify each recorded original hash with
   `git show OLD_REVISION:SOURCE_PATH | sha256sum`, and each local derived
   hash, before changing the baseline.
3. Review `git diff OLD_REVISION NEW_REVISION -- SOURCE_PATH` for every
   selected file.
4. Inspect the full upstream diff too, including shared macros, attributes,
   contexts and selection makefiles: a security fix may live outside the
   current selection. Check both repositories together.
5. Merge relevant changes against the old upstream file and the derived file.
   Do not overwrite the adaptations.
6. Trace each added allow to its current provider and resource. Include new
   files only when their consumers or shared definitions are required. Keep
   the upstream headers.
7. Update the upstream revisions, source hashes and local hashes in
   `provenance.json`. Review removals and renames explicitly.
8. Publish a new device commit and update the pinned build environment and
   project map.
9. Compile the release and recovery policy with neverallows enabled, inspect
   the effective grants and negative privilege checks, and repeat the affected
   device tests.
