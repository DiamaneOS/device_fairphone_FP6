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

- The policy is enforcing; compatibility tests and neverallow checks stay on.
- Never fix a failure by importing stock policy wholesale or by adding a
  permissive domain.
- Bind hardware grants to the service domain, not to a HAL attribute. Keep the
  HAL IPC attributes a service needs.
- Adding a service means reviewing both its activation and its grants.
- Explain every `dontaudit` and every broad rule, here or next to the rule.
- Use platform init and ueventd permissions where they already cover an
  operation.
- Firmware-handler transitions are not imported; review them if the product
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
- The peripheral manager has no binder call into the WLAN service domain; that
  service is not installed.
- `vendor_per_mgr` may use QRTR sockets (`qipcrtr_socket`, no ioctls): its QMI
  libraries open AF_QIPCRTR sockets.
- No label or ueventd rule for `/dev/qseecom`: the legacy QSEECom driver is
  not shipped. `/dev/smcinvoke` is the only `tee_device` node.
- No QSEECom AIDL proxy (`vendor.qti.hardware.qseecom@1.0-service`):
  `hal_qseecom.te` and its file and service contexts are not imported.
  - No installed file looks it up. KeyMint, Gatekeeper, the fingerprint module
    and `qseecomd` use `libQSEEComAPI` directly.
  - The kernel `qseecom_proxy.ko` is a different component and stays.
- No secure-processor (SPU) grants for the gatekeeper HAL and `qseecomd`: the
  FP6 has no SPU, so those device nodes never exist.
- The FP6 fingerprint service
  (`android.hardware.biometrics.fingerprint-service.fp6`) is
  `hal_fingerprint_default_exec` (`vendor-common/file_contexts`).

## Device grants (`fp6/`)

### Sensors

`sensors.te` and `hal_sensors_default.te` are reduced from the Qualcomm files
of the same name (`provenance.json`).

- `vendor_sensors` (sscrpcd): FastRPC through the secure node only
  (`vendor_xdsp_device`), the system DMA-BUF heap and wake locks.
  - It keeps the sensor registry and calibration on persist, and state in
    `/data/vendor/sensors`.
  - No capabilities, QRTR or sensor-device grants.
- `hal_sensors_default`: QRTR without ioctls, FastRPC through the secure node,
  the system DMA-BUF heap and the graphics allocator (direct channel).
  - It reads `persist/sensors` and the sensor, FastRPC and AON properties.
  - Not granted: capabilities, diag, sysrq, SLPI/SSR sysfs, HID devices and
    persist writes.
- `sensors_list.txt`, the sensor types the HAL waits for at start, has its own
  label, `vendor_persist_sensors_list_file`.
  - init copies it from vendor at every boot: the stock list without the hall
    sensor (`boot/init.qcom.rc`).
  - The HAL may read it; its attempts to append to it stay denied.
- `dontaudit hal_sensors_default property_socket:sock_file write`: the sub-HAL
  publishes every proximity sample as a factory-test property that nothing
  reads.
  - The set stays denied. Not auditing it avoids a flood of records while the
    proximity sensor is on.

### Graphics

- Every domain except isolated apps may search the GPU sysfs directory and
  read the GPU model: the Adreno driver runs inside every process that
  renders (`graphics.te`).
- `vendor.gralloc.*` (`vendor_gralloc_prop`) is readable by the processes that
  map buffers, not by every domain.
  - These are non-isolated apps, zygote, system_server, bootanim,
    SurfaceFlinger, mediaswcodec, cameraserver, mediaserver, the camera
    provider, the codec service, the allocator and the composer.
- The Adreno read-only properties (`vendor_public_vendor_default_prop`) are
  readable by the same set without cameraserver, mediaserver and the camera
  provider.
- `libllvm-qgl.so` is a same-process HAL library (`file_contexts`).
- The composer keeps state in `/data/vendor/display`.
- init may `setattr` `vendor_sysfs_graphics` files: `init.qcom.rc` hands the
  panel brightness node to system.
- The DisplayPort and audio-codec extcon devices are `sysfs_extcon`, which
  system_server reads to detect DisplayPort (HDMI) and wired headsets.
  - They are labelled by device path, not by extcon index: extcon numbers
    follow probe order.
  - No vendor service reads extcon.

### Fingerprint

- `/dev/focaltech_fp` is `ff_device` (stock label).
- The fingerprint HAL may open, read, write and ioctl that node and QSEECom
  (`tee_device`), and open, read and ioctl the two QSEECom heaps
  (`fingerprint.te`).
- Templates live in `/data/vendor_de/<user>/fpdata`
  (`fingerprint_vendor_data_file`), which the platform `hal_fingerprint` rules
  cover.
- The stock module registers a factory binder service at start. The HAL
  answers that registration inside its own process
  (`fingerprint/ModuleFactoryService.h`), so it never reaches servicemanager.
  - The service has no label. It would fall to `default_android_service`,
    which the platform forbids granting.
- Neverallow: the HAL may not open or read `vendor_data_file` files, where the
  module's configuration and dumps would live.
- Neverallow: only init, vendor_init and vold_prepare_subdirs may create
  directories with the HAL's data label or relabel directories to it; only
  init and vendor_init may relabel files to it.

### Hardware identifiers

- `/sys/devices/soc0/serial_number` is `vendor_sysfs_soc_serial`
  (`soc_serial.te`), not the soc0 directory's `vendor_sysfs_soc`.
- No vendor rule grants it: no shipped program reads it. The composer's
  imported read of all sysfs excludes it.
- The type must keep `sysfs_type`, which the platform lets some root daemons,
  the Bluetooth, Wi-Fi, supplicant and radio HALs and tee read.
  - ueventd makes the node 0400 root, so of those only the root platform
    daemons can open it.
- A neverallow keeps every other domain off it.
- In soc0 the thermal HAL and the thermal engine read only the public `soc_id`
  and `hw_platform` (`platform.te`).
- The UFS serial number and its LUNs' SCSI serial and identification pages
  keep the generic `sysfs` label, which many domains may read.
  - ueventd makes them 0400 root too, and no shipped program reads them.

### Thermal

- Thermal HAL: runs as system with no capabilities
  (`power/init.fp6.thermal.rc`).
  - ueventd gives group system the two trip nodes it writes
    (`trip_point_1_temp` and `trip_point_1_hyst` of each thermal zone).
  - From `fp6/` it gets the public SoC id and the thermal zones, read-only.
- Thermal engine (stock, runs as root): keeps its imported domain
  (`vendor-common/thermal-engine.te`).
  - From `fp6/` it gets the public SoC files and the relabelled CPU and GPU
    limit nodes (see [Power](#power)).

### Storage, wakeup sources and tuning

- The UFS LUN 0 block directory is `sysfs_devices_block`, which init may only
  read.
- The one node platform init.rc writes there, the root disk's
  `queue/discard_max_bytes`, is `vendor_sysfs_ufs_discard_max`, writable by
  init only (`storage.te`).
- The wakeup sources of the fingerprint, hall, privacy-switch and NFC devices
  are `sysfs_wakeup`, which the suspend service reads.
- `init.fp6.perf.rc` writes WALT and VM sysctls that the platform does not
  label. Each has its own type, and only vendor_init writes them (`perf.te`):
  - `/proc/sys/walt`: `vendor_proc_walt`;
  - `swappiness` and `min_free_kbytes`: `vendor_proc_vm_tuning`;
  - `compaction_proactiveness`: `vendor_proc_compaction`.
- vendor_init also writes the platform `proc_sched` and
  `proc_watermark_scale_factor` sysctls. It has no write on generic `proc`.

### Wi-Fi

- The Wi-Fi HAL may write `/dev/wlan` to switch the driver on, and set the
  driver version property (`platform.te`).
- `/proc/sys/net/ipv4/tcp_limit_output_bytes` is
  `vendor_proc_tcp_limit_output_bytes`. Only the Wi-Fi HAL reads and writes
  it, for high-throughput TCP tuning.
  - The rest of `/proc/sys/net` stays read-only to it.
- `dontaudit hal_wifi_default self:capability sys_module`: creating the
  hotspot interface makes the kernel try to load a module named after it.
  - The driver is already loaded, and module loading is never granted. AOSP's
    netd has the same rule.
- `/data/vendor/tombstones` is not labelled as a whole (only `rfs/`), so the
  platform `tombstones/wifi` label for the HAL's ring-buffer logs applies.
- The factory MAC file the driver loads (`vendor_diamaneos_wlan_mac_file`,
  written by imeiprovd) is read only by ueventd, which serves it as firmware
  (`wlan_mac.te`).
  - For that, ueventd may search `/mnt/vendor` and the MAC directory. This
    policy gives it nothing else under `/mnt/vendor`.
  - A neverallow keeps every domain but init, vendor_init and ueventd from
    reading the file.

### Touch and vibrator

- The touch controller's wake-gesture switch (`gesture_wakeup`) is
  `vendor_sysfs_touch_gesture` and its tap report (`wake_gesture`) is
  `vendor_sysfs_touch_wake_gesture`. The rest of the touch device keeps the
  factory-test type `fp_mmitest_sysfs`.
  - Only the sensors HAL uses them, for Tap to wake (double tap) and Tap to
    check phone (single tap): it writes the switch and reads the report
    (`fp6/touch.te`).
  - The HAL may search the device directory but not use its factory-test
    files.
  - The power HAL no longer has access.
- The Awinic haptics nodes the vibrator HAL writes (`activate`, `brightness`,
  `duration`, `gain`, `loop`, `seq`) are `vendor_sysfs_aw_vibrator`
  (`vibrator.te`).
  - The rest of the driver stays `sysfs_leds`, read-only to the HAL: raw
    waveform streaming (`rtp`), calibration and the other LEDs.
  - genfs matches prefixes, so `activate_mode` gets the type too. init does
    not hand that node to system, so the HAL still cannot write it.
  - The HAL has no `input_device` access: it can neither read touch and key
    events nor drive the chip's force-feedback input node.
  - No grants for the absent `qcom-haptics` node or persist haptics
    calibration.

### USB-C port control

vendor_init writes two nodes from the `sys.port_security_mode` and boot
triggers in `boot/init.qcom.usb.rc` (`usb_port_security.te`).

- The USB controller's `dynamic_disable` (data off, charging kept) is
  `vendor_sysfs_usb_data_disable` (`vendor-common/genfs_contexts`).
  - It is split from `vendor_sysfs_usb_device`, which also covers the
    controller role.
  - The USB HAL may not write it: with port control on, the framework never
    passes `enableUsbData` to the HAL.
- The charger firmware's input suspend
  (`/sys/class/qcom-battery/suspend_input_current`, which Off sets) is
  `vendor_sysfs_usb_input_suspend` (`fp6/genfs_contexts`).
- Neverallows keep every other domain from writing either, except ueventd,
  which the platform lets write all of sysfs.

### Embedded USB debugger

- The switch that turns Qualcomm's embedded USB debugger (EUD) on,
  `/sys/module/eud/parameters/enable`, is `vendor_sysfs_eud_enable`
  (`eud.te`).
- A neverallow leaves only ueventd and vendor_init able to write it; the
  platform lets those two write all of sysfs.
- Reads stay as the platform grants them: the value only tells whether EUD is
  on.

### Other grants

- The peripheral manager and its proxy read the remote-processor names
  (`vendor_sysfs_ssr`).
- KeyMint reads the vendor patch level, for key attestation.
- vendor_init sets the vendor properties that the vendor init scripts and
  build properties set: gralloc, display, USB, SPU, audio, radio, subsystem
  restart, data profile and sensors (`platform.te`).

### Not granted

- `qseecomd` on the raw UFS LUN 0 node: a platform neverallow on `device`
  forbids it. RPMB and LUN 4 have their own labels.
- `fsck` on `vm-bootsys`: nothing on the FP6 mounts `/product/vm-system`.

## system_ext and product rules

- `product-private/property_contexts`: `debug.disable_screen_decorations` is
  `vendor_display_notch_prop`, which SystemUI cannot read.
  - The read falls back to false, so the privacy dot cannot be switched off
    with `adb setprop`.
- `system-ext-private/system_app.te`: Settings reads
  `ro.vendor.build.security_patch` (`vendor_security_patch_level_prop`) for
  its "Vendor security update" row.
  - The platform grants that property to shell, keystore and vendor_init only.
- `system-ext-private/privacy_switch.te`: the kernel block of the Moments
  privacy switch.
  - init writes `/sys/kernel/privacy_switch/policy` once per boot; the kernel
    refuses later writes.
  - Neverallows keep every domain but init, vendor_init and ueventd from
    writing the switch's sysfs files.
  - system_server reads the kernel's state. Only it and init set
    `privacy_switch_prop`; Settings reads it.
- `system-ext-private/vold_adoptable.te`: vold may list `/data/misc_ce` and
  `/data/misc_de`.
  - It finds there the users whose keys it destroys when an adopted microSD
    card is forgotten.
- `system-ext-private/fsck_untrusted.te`: `fsck_untrusted` may use the
  bootstrap libraries and two read-only block ioctls, for the e2fsck run on an
  adopted card before it is mounted.
- `telephony-system-ext/property_contexts`: gives a telephony polling
  property and public `Build.VERSION` properties existing platform types, so
  apps are never granted `default_prop`.

## Firmware release

`firmware/` is the policy of fwrelease (device `firmware/`), which reports the
installed Fairphone firmware release to Settings.

- `vendor_fwrelease` runs as its own user without capabilities.
- It may only open and read the firmware partitions and set
  `ro.vendor.diamaneos.firmware_release`.
- The property is vendor restricted. Settings (`system_app`) reads it, as do
  init, vendor_init and dumpstate through platform grants; shell does not.
- The firmware partitions that would share `vendor_custom_ab_block_device`
  with vbmeta, dtbo, pvmfw, multiimgqti and qweslicstore have their own type,
  `vendor_firmware_image_block_device`, so fwrelease cannot open those.
  - The type is set in the UFS lines of `vendor-volcano/file_contexts`.
  - It has the same grants: update_engine read-write, fastbootd read-write in
    recovery, init relabel, boot control getattr.
  - modem, bluetooth, uefi, uefisecapp and xbl keep their own types.
- ueventd makes the partition nodes of both slots 0640
  root:vendor_fwrelease (`boot/ueventd.rc`).
  - The XBL LUNs share `vendor_xbl_block_device` but stay 0660
    root:vendor_bootctl, so fwrelease cannot open them.
- Neverallows for fwrelease: no capabilities, block writes or ioctls, network
  sockets or other properties.
- Neverallows for the property: only fwrelease, init and vendor_init set it;
  only the readers above read it.
- Who reads the firmware partitions is an image check, not a neverallow: init
  and the recovery domains read all block devices through platform grants.

## Audio

- `audio/adsprpcd.te` and `audio/hal_audio_default.te` are reduced from the
  Qualcomm files of the same name (`provenance.json`).
- The audio file contexts and init-directory rules are DiamaneOS's.
- The primary HAL, PAL and AGM are built from source (device
  `audio/provenance.json`) and keep the stock domains and labels.
- The policy confines device access to the audio service domains.

### `hal_audio_default`

- AGM device, ION and system DMA-BUF heap, the runtime audio directory
  (`/data/vendor/audio`) and the audio properties.
- Sound-card state node: read and write. AGM and PAL open it read-write;
  without write no audio comes up.
- ADSP sleep monitor: only the activity-reporting ioctl (`0x5201`).
- QRTR: its own `qipcrtr_socket` with create, read, write, getattr and setopt
  (no ioctl, bind or connect).
  - GPR and GSL use it for the audio-PD lookup and restart notifications.
  - SELinux cannot filter QRTR by service, so the HAL can send QMI to any
    service.
- Amplifier factory calibration on persist: read-only.
  - At each speaker start PAL reads the per-unit calibration and writes it to
    the DSP.
  - The kernel needs no access to `/mnt/vendor`, which a platform neverallow
    forbids.
- HIDL: the HAL registers the AGM service (`IAGM`) in its own process, and no
  PAL service.
  - `qva-common/hwservice_contexts` labels `IAGM` and `IPAL`
    `vendor_hal_audio_internal_hwservice` (`audio/hwservice.te`).
  - A neverallow lets only `hal_audio_default` look them up. audioserver,
    system_server and the Bluetooth stack cannot reach them.
- Not granted, unlike upstream: diagnostic devices, sensor persist writes,
  voice-UI sockets, QTR SDK access, broad HAL-attribute grants, vendor Binder
  use and DSP restart controls (`vendor.audio.crash.trigger` is not set).
- PAL's wake locks stay denied (see
  [Not granted on purpose](#not-granted-on-purpose)).

### `vendor_adsprpcd`

The FastRPC listener, audioadsprpcd.

- Its DSP transport nodes and the system DMA-BUF heap.
- Firmware and RFSA files, read-only.
- Audio-DSP state in `/data/vendor/audio_dsp`.
- A lookup of the DSP manager hwservice.

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

- QRTR sockets: create, connect, getattr, read, setopt and write; no ioctl or
  bind.
- It lists `/sys/class/remoteproc` and reads each processor's `firmware`
  attribute (`vendor_sysfs_rproc_firmware`, `modem/file_contexts`).
- Read-only requests (`modem_pr/...`) are served through the
  `/vendor/firmware/modem_pr` link to the modem partition.
- Its read-write directory is `/data/vendor/tmp/tqftpserv`
  (`vendor_tqftpserv_data_file`).
  - It may open, create, read, write and delete files there; no rename, link,
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
- Neverallows: only init and vendor_init set it; only they, dumpstate and the
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
- SELinux alone would let the HAL use them; ueventd keeps the three root-only.

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
  - the peripheral manager and vndbinder;
  - the location daemons' sockets (loc_launcher, XTRA, LOWI, Wi-Fi
    crowdsourcing, sensor, correction, engine and Skyhook services);
  - RIL and SSG sockets, MHI sysfs and QMS/AON properties.

## Time keeping

`timekeep/` holds the policy of DiamaneOS's time daemon. It replaces
Qualcomm's `time_daemon`, whose policy, file label and `tee` socket rule are
not imported.

- `vendor_timekeepd_restore`: a one-shot step at post-fs-data. It sets the
  clock from the RTC counter plus the saved offset.
  - It is the only one of the two domains with `sys_time`, and it cannot write
    the offset file.
- `vendor_timekeepd`: long-running, with no capability.
  - It searches `/sys/class/rtc` and reads the RTC counter file `since_epoch`
    (its own genfs type).
  - It replaces files in `/data/vendor/timekeepd`.
- Neither has sockets, binder, properties, `/dev/rtc0` or persist.
- A neverallow keeps other vendor domains from writing the offset file.

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
    (`vendor_sysfs_cpufreq_limit`, `fp6/genfs_contexts`);
  - the GPU devfreq `min_freq` and `max_freq` and the GPU wake trigger
    `touch_wake` (`vendor_sysfs_kgsl_limit`, `fp6/file_contexts`);
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
    (`vendor_power_sched_boost_prop`, values 0, 1 and 2); vendor init writes
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
  - reads of the nodes (dumpsys shows request indexes, not values);
  - the debug configuration in `/data/vendor/etc`
    (`vendor.powerhal.config.debug`);
  - the Pixel-only `/proc/vendor_sched`;
  - the display `idle_state` nodes: the Qualcomm display driver has none
    (`vendor.powerhal.disp.idle_support=false`);
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
  Qualcomm's vendor policy; `telephony-system-ext/vendor_qtelephony.te` and
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
- It is not a `binderservicedomain`; each selected app client has an explicit
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
- FastRPC with read-only opens: the secure node (the ADSP sensors PD of the
  CamX sensor direct channel), the non-secure node (the CDSP offloads) and
  `/vendor/dsp`.
- Qualcomm DMA-BUF heaps; on the display heap only the allocation ioctl.
- SoC, camera, JPEG and DDR identification, `/proc/meminfo` and the public SoC
  id.
  - It may search `/sys/devices/soc0` for the per-part files
    (`num_subset_parts` is labelled in `camera/genfs_contexts`).
- QRTR sockets without ioctls, for the gyro QMI client.
- The thermal-engine client socket.
- The provider's own vndbinder open; no vndservice lookup.
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

- It exposes the hardware encoders. It exposes the non-secure hardware
  decoders only when the user turns on Hardware video decoding (off by
  default; `media/media.mk`).
- Every app except isolated processes is a Codec2 client and can call it.
- The platform vendor policy makes `mediacodec` the Codec2 HIDL server and a
  gralloc and GPU client, and gives it every `video_device` node.
  - That type also labels the camera V4L2, subdevice, media, JPEG and CVP
    nodes, so the DAC group (`mediacodec`) is what keeps the codec off them.
- Codec nodes: `/dev/video32` (decoder) and `/dev/video33` (encoder) have
  their own type, `vendor_video_codec_device`.
  - Only `mediacodec` may open them (neverallow), with the V4L2 ioctls only
    (`video_ioctl2` is the driver's only handler).
  - The decoder node is root-only unless decoding is on.
  - `vendor_init` changes its user and mode at zygote-start, through the
    platform's setattr grant on every device node, and may `getattr` it.
- `persist.diamaneos.hw_video_decode`: the user's choice; accepts only 0
  and 1.
  - Only Settings sets it (`system-ext-private/hw_video_decode.te`).
  - It is public and system restricted, because the vendor rc triggers on it.
- `ro.vendor.diamaneos.hw_video_decode`: the state of the running boot;
  accepts only 0 and 1.
  - Only init and `vendor_init` set it; the codec service and Settings read
    it.
- No network: the platform neverallows cover tcp, udp and rawip sockets.
  - DiamaneOS's seccomp filter (`media/seccomp`) fails every socket family but
    AF_UNIX with EPERM, and the stock one has no `socket` at all.
- Other grants: `SYS_NICE`, the gralloc and Adreno properties, and
  `vendor.media.target_variant`.
  - Through that property the service finds the target specification that
    limits the codecs it registers.
  - `vendor_init` sets it from the vendor build.prop and switches it when
    decoding is on.
- `dontaudit` on the lookup of the composer's `IDisplayConfig` (refresh rates
  for perf hints, which are off): the service exposes screen writeback and
  panel controls, so the lookup stays denied.
- `dontaudit` on the FastRPC nodes, with a neverallow on opening a DSP node,
  as upstream: `libfastcvopt` then runs on the CPU.
- Not granted, unlike the stock policy:
  - the Codec2 audio service, Wi-Fi display, VPP, hexlp and the capability
    config store;
  - secure video (content-protection and membuf nodes, QSEECom);
  - the performance HAL and `/data/vendor/media`;
  - the Qualcomm system DMA-BUF heap: the selected libraries allocate from the
    AOSP system heaps only.
- There is no provenance entry for these files.

## Recovery

- The recovery executable uses the boot-control service over IPC; it does not
  link the legacy Qualcomm updater.
- So this policy gives recovery no raw SCSI/BSG access and no CAP_SYS_RAWIO.
- Boot control keeps BSG access and CAP_SYS_RAWIO for the UFS implementation.
- Legacy sg character-device access is not granted; the sysfs discovery used
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
  set on this build; CamX uses the public SoC id file.
- `hal_bluetooth_default`, `hal_camera_default` and `vendor_hal_gnss_qti`,
  `ro.vendor.qti.va_aosp.support` and `va_odm.support`: not set on this build,
  so a denied read returns the same default.
- `hal_audio_default` and `rild`, `persist.vendor.pd_locater_debug`
  (`vendor_pd_locater_dbg_prop`): a debug switch, off when unreadable. No
  domain reads it here.
- `vendor_nicmd`, `property_socket` write and an `init.svc.*` read: the SHS
  and QMI-priority helpers it would start and check are not installed.
- `vendor_nicmd`, `netlink_route_socket` `nlmsg_readpriv`: the stock policy
  does not grant it either.
- `vendor_nicmd`, `rawip_socket` `create`: the stock policy does not grant it
  either, and Wi-Fi calling works without it.
- `fp6_iwlan_app` and `mediacodec`, reads of `default_prop` and
  `zygote_config_prop`: generic platform properties that a denied read
  returns as unset. Neither needs them.
- `hal_camera_default`, finding
  `vendor.tcl.camera.algoservice.ITctCameraAlgoService`: the TCL algorithm
  service is not installed, and CamX continues without it.
- `tee` (qseecomd), opening the GPT, XBL and boot block devices and the BSG
  nodes of the other UFS LUNs: it only probes them at start.
- `fp6_callaudio_app` and `fp6_iwlan_app`, `find` of `gpu_service` and
  `netstats_service` (and `content_capture` for the bridge): lookups every app
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
5. Merge relevant changes against the old upstream file and the derived file;
   do not overwrite the adaptations.
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
