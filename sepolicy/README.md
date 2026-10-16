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
    ring-buffer logs applies.
  - The genfs lines for these tuning nodes are in `fp6/genfs_contexts`.
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
  grants it only to the PD mapper.
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
The HAL's `property_set` of `vendor.nfc.fw.version` (`vendor_nfc_prop`) stays
denied: `nfc_nci_sec.so` ignores the result and never reads it back, and the
only stock reader is the factory MMI app. If the firmware version is wanted in
bug reports, the minimal rule is
`nfc/hal_nfc_default.te`. The HAL's read of `persist.sys.factory.mode`
(`system_prop`) also stays denied, which keeps it on the normal firmware path.
`/data/nfc`, which platform init.rc creates for the NFC stack's state, is
`nfc_data_file` (`system-ext-private/file_contexts`, as in stock).
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
