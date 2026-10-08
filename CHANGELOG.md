# Changelog

## Unreleased

- The call-audio bridge sends an audioserver status again, once a second,
  when the radio daemon did not receive it but is still registered (a oneway
  call also fails while the daemon's binder buffer is full). A lost "server
  died" goes before the following OK, on which the daemon re-sends the call
  state, so in-call audio recovers after an audioserver restart. Not yet
  built.
- The fingerprint HAL skips enrollment id 0 in `removeEnrollments`: the
  legacy module reads it as "all of the user's fingerprints", so a caller
  passing 0 removed every enrollment. Not yet built.
- Init waits for `ssr_setup` at early-boot, before the ADSP, CDSP and WPSS
  boot, so subsystem restart is on before any remote processor starts. It
  was started without waiting, after the DSPs had already booted: a crash in
  that window panicked the phone. The wait is bounded to 5 s; the property
  trigger runs it again after boot. Not yet built.
- The display colour manager's tinyxml2 (`compat/tinyxml2-v34`) takes
  upstream's character-reference fix: each digit is checked before it is
  added, so an overlong numeric reference is rejected instead of wrapping
  around to another character (`provenance.json` records the backport).
  Not yet built.
- The fingerprint HAL rejects a press-to-auth parcelable whose size would
  overflow the parcel position, instead of aborting in the integer overflow
  sanitizer. Not yet built.
- A fingerprint session that was closed or replaced, or whose client died,
  refuses new requests with `EX_ILLEGAL_STATE` instead of still driving the
  sensor module and disturbing the active session. Not yet built.
- The fingerprint HAL no longer frees its client-death cookie twice when
  linking to the client fails (libbinder_ndk already frees it), and refuses
  a session whose client has already died before it touches the module.
  Not yet built.
- Camera privacy (the Moments switch, the camera access toggle) disconnects
  camera apps and refuses new opens instead of muting, AOSP's path for
  cameras without mute support (`ro.camera.disableCameraMute`, read by the
  camera service). CamX's test-pattern mute failed while streaming and, with
  the kernel camera floor, crashed in a loop. Not tested on the phone yet.
- The framework overlay points the Moments kernel floor at the switch
  driver's `state` file, so Android shows a hardware microphone block only
  while the kernel blocks (system_server already reads that directory).
  Userdebug and eng builds add AGM's `agmcap`, `tinymix` and a two-entry
  `backend_conf.xml` for capturing the microphones past Android in the kernel
  floor test; user builds ship none of them.
- Seal the Moments switch's kernel microphone block once per boot. A
  system_ext init script writes the owner's choice to the privacy switch
  driver (`/sys/kernel/privacy_switch/policy`) after post-fs-data; the kernel
  refuses later writes. Only init may write the node and only system_server
  sets the policy property (`sepolicy/system-ext-private/privacy_switch.te`).
  The framework overlay names the kernel's status file. Not built into a
  kernel or tested on the phone yet.
- The camera provider can no longer write `/data/vendor/camera/coredump`
  (owned by system, mode 0500): CamX created an empty dump folder there for
  every `dumpsys media.camera` even with its core dumps turned off.
- The camera provider leaves tombstones and stack dumps again: under its
  seccomp jail (no_new_privs), debuggerd uses the in-process fallback handler,
  which SELinux denied (`crash_dump_fallback(hal_camera_default)`, as for
  rild).
- Build the AudioReach primary HAL, PAL, AGM with its HIDL service and ALSA
  plugins, and audioadsprpcd from Fairphone's published FP6 sources
  (DiamaneOS forks, `audio/provenance.json`) instead of shipping the stock
  blobs, under the same names and with CFI and the integer overflow
  sanitizer as stock. The HAL no longer registers the PAL HIDL service, so
  `IPAL` leaves the manifest and the framework matrix. The graph services
  (only their headers are published), their tuning server, the voice UI
  interface, the deadline manager, calibration and configuration stay
  stock. Adds `misc/adsp_sleepmon.h` to the kernel UAPI headers. SELinux
  domains and labels unchanged. Checked on the phone.
- For the hardened kernel: the EUD debugger's module is no longer loaded (the
  kernel ships without it), and init no longer writes `download_mode` (the
  kernel makes it read-only; panic dumps are off by default). Checked on the phone.
- Log what the closed thermal engine changes, on userdebug and eng builds
  only: `sepolicy/fp6/thermal_engine_audit.te` adds "avc: granted" records
  for each node it opens for writing, the device nodes it opens, the sockets
  it creates, its capability use and its shutdown requests. No access
  changes; user builds are unchanged. Checked on the phone.
- Use the factory Wi-Fi MAC as the driver's hardware address, as stock does,
  instead of the chip's generic Qualcomm one (prefix 00:03:7f; -180).
  - imeiprovd writes the driver's MAC file from the traceability partition
    at post-fs, into a RAM-backed tree; ueventd lists that tree as a
    firmware directory (`boot/ueventd.rc`) and is the only service that may
    read it (`sepolicy/fp6/wlan_mac.te`).
  - Android still randomises the address per network. Its stored factory
    MAC changes only after the stored value is cleared or the phone is reset.
  - Checked on the phone.
- User builds silence the CamX log tag: the closed camera provider logged the
  camera modules' serial numbers at every start. Debuggable builds keep CamX
  logs. Tested by setting the property by hand: no serial lines, camera works.
  Checked on the phone.
- Call volume has 15 steps, as media, instead of 5 spread over 15 (the volume
  keys jumped 1, 5, 8, 12, 15). Checked on the phone.
- Enforce the Bluetooth HCI service's seccomp filter: a call outside its list
  now stops the service (SIGSYS, tombstone; init restarts it) instead of only
  being logged. In log mode, pairing, music (AAC), a headset call (mSBC),
  on/off cycles and scans logged no call outside the list. Checked on the phone.
- Use the factory Bluetooth address. imeiprovd now reads it from the
  traceability partition at boot and sets
  `ro.vendor.diamaneos.bt.factory_address`; `init.fp6.bluetooth.rc` copies
  it to `ro.vendor.bt.boot.macaddr`, as stock's `tctd.rc` does, which the HCI
  implementation reads before falling back to its stored generated address
  (prefix 22:22). The HAL property gets its own type, read only by the HAL
  and set only by vendor_init. Existing pairings may need pairing again once.
  Checked on the phone: the adapter uses the factory address.
- Compress zram with lz4 instead of zstd. Measured over two 20-app relaunch
  rounds each: relaunch median 137 -> 118 ms, memory stall 0.78 -> 0.47 s,
  same kills; costs a lower compression ratio (about 4 instead of 6) and a
  little more kswapd CPU.
- The source-built gralloc no longer tries to load `libubwcp.so` when UBWC-P
  support is compiled out (`hardware/qcom/display`), so apps, system_server,
  SurfaceFlinger and the media services no longer log its denial; removed
  from the expected-denials list. The camera provider still loads it itself.
  Built.
- Give the switch that turns Qualcomm's embedded USB debugger (EUD) on,
  `/sys/module/eud/parameters/enable`, its own SELinux type with a
  neverallow: only ueventd and vendor_init keep write access (the platform
  grants them all sysfs types); vold, the USB HAL and vfio_handler lose it.
  The debugger stays off. Built.
- Drop stock's `wowlan_triggers=magic_pkt` from the station supplicant
  overlay. The magic-packet wake-up itself is turned off in the driver
  configuration that the tools generate (`gEnableWoW=2`). Checked on the phone.
- Remove the imported hwservice contexts of the ANT, ANT HCI and Bluetooth
  SAR interfaces, whose libraries are not shipped: the Bluetooth HAL could
  still register them under its own label. Built.
- Run the boot control HAL as its own user, `vendor_bootctl`, with
  CAP_SYS_RAWIO only (stock: root with every capability). ueventd gives the
  group the GPT disks of the A/B LUNs (sdb, sdc, sde), misc and the UFS BSG
  node; the other LUNs stay root-only. Needs the matching
  `hardware/qcom/bootctrl` change. Checked on the phone.
- Build the dm-verity hash trees of system, system_ext, product, vendor,
  odm, vendor_dlkm and system_dlkm with SHA-256 instead of avbtool's SHA-1
  default, as stock does. Checked on the phone.
- Run the Bluetooth HCI service under a seccomp filter. The service compiles
  `bluetooth-hci.policy` (`bluetooth/seccomp`) and installs it at the start
  of main(), before it loads Qualcomm's closed HCI implementation, so the
  whole process is covered: threads but no child processes, Unix sockets
  only (other socket families fail), no writable and executable mappings,
  and kill only as SIGKILL, which the implementation sends itself after a
  controller failure. The list comes from the imports of every library in
  the process. For now calls outside it are logged (kernel audit record type
  1326) and allowed; `seccomp_audit.py --service bluetooth` turns those
  records into policy additions. The service aborts if the filter cannot be
  installed. Built.
- Add DiamaneOS's eSIM manager (DiamaneOSEuicc), which manages the profiles on
  the eUICC: list, turn on and off, rename, delete. It ships disabled; the eSIM
  support switch in Settings turns it on and restarts the phone. Mark physical
  slot 1 as a built-in eUICC (`non_removable_euicc_slots`), which stock leaves
  unset. Checked on the phone.
- Use AOSP's software audio effects only: our own `audio_effects.xml` lists
  the AOSP bundle, reverb, visualizer, downmix, loudness and dynamics
  effects without DSP offload halves or the effect proxy, so apps' effect
  parameters no longer reach Qualcomm's closed offload bundle and
  visualizer, which are no longer shipped. Qualcomm's VoIP AEC/NS
  descriptors (the DSP's echo cancellation) and volume listener are built
  from unmodified CodeLinaro audio-ar sources (`audio/effects`) under
  their stock names. Checked on the phone.
- Lower the GPU floor for EXPENSIVE_RENDERING from 763 to 510 MHz: GPU
  composition rarely needs the pin, the GPU governor still clocks up when
  busy, and 510 MHz runs at a lower voltage. Built.
- Let ART compile on the phone for the actual cores (runtime CPU variant
  cortex-a55: LSE atomics, FP16, dot product, no Cortex-A53 workarounds), as
  on Pixel 9, which also has A720 and A520 cores. Built.
- Keep the stock Bluetooth HCI implementation's two firmware-download tags at
  warning level: at info level they logged the Bluetooth address each time
  Bluetooth started, and at debug level HCI command dumps. Built.
- Add neverallows that keep every domain except init, vendor_init and
  vold_prepare_subdirs from creating or relabelling files and directories to
  the fingerprint HAL's data label, so no other process can plant a
  configuration the closed fingerprint module would read. No rule changes.
- Remove the imported policy of the display colour service, which is not
  installed or declared: its domain, executable label and service contexts,
  and SystemUI's client grant to it. Built.
- Drop the imported secure-processor (SPU) grants of the gatekeeper HAL and
  qseecomd: the FP6 has no SPU, its drivers are not shipped and the selected
  gatekeeper names no SPU node, so those device nodes never exist. Not yet
  built.
- Make the UFS storage's serial number and its LUNs' SCSI serial and
  identification pages root-only through ueventd. No shipped program reads
  them, and their generic sysfs label is readable by 22 system and vendor
  domains, among them the fingerprint HAL and the composer. Checked on the phone.
- Document why four denials stay denied (the audio HAL's kernel wake locks,
  which only sound trigger takes; SystemUI's read of an LE audio property
  that is not set; the IWLAN and call-audio apps' start-up lookups of the GPU
  and network statistics services) and why nicmd needs to read XFRM state:
  it removes the Wi-Fi calling security associations it installs by dumping
  all of them.
- Run the thermal HAL as system without capabilities instead of root, as the
  Pixel thermal HAL runs. ueventd gives group system the two trip nodes it
  writes; they stay root-owned for the thermal engine. Checked on the phone.
- Give the SoC serial number its own SELinux type and make it root-only. No
  shipped program reads it; the composer's read of all sysfs now excludes it,
  the thermal engine no longer reads the soc0 files beyond the public ids,
  and a neverallow keeps vendor services off it. The platform still lets some
  HAL domains read all sysfs, but those run as their own users. Checked on the phone.
- Raise the ADPF uclamp ceiling from 384 to 512: 384 is below the capacity
  of the A520 little cores (about 454), so a hint session running over its
  target, SurfaceFlinger included, could never move to a bigger core. Normal
  frames keep the lower starting values. Built.
- Remove the camera streaming modes' little-core caps from
  `powerhint.json`: CamX runs on the little cores, so the caps would slow it,
  and nothing sends those modes. CAMERA_SHOT now also sets `sched_boost`, like
  CAMERA_LAUNCH, so capture work can leave the little cores. Built.
- Pass the stock camera's performance hints to the power HAL. CamX's open,
  close and snapshot hints become CAMERA_LAUNCH and CAMERA_SHOT boosts of at
  most 5 s (a hint held until release at most 2 s), ended early when CamX
  releases them; `libqti-perfd-client` sends them from its own thread with
  one-way calls, so the camera never waits for the power HAL. The camera
  provider becomes a power HAL client, as on Pixels. Built.
- Ask CamX for a UBWC preview stream: the display hardware rotates only UBWC
  buffers, so the linear portrait preview was composed by the GPU on every
  frame. The 16:9 preview is now composed by the display hardware.
- Allow the calls the camera provider's seccomp log showed: at start
  prctl(PR_GET_DUMPABLE) and sched_get_priority_min (read-only queries), and
  during capture setpriority for its own threads (PRIO_PROCESS only) and
  getrlimit.
- Label `/data/vendor/tzstorage` as the TEE's storage again, as upstream: the
  line was lost when the Qualcomm file contexts were reduced, so qseecomd,
  which may write that type, could only read the directory.
- Let imeiprovd read the traceability partition: ueventd gives its block
  device to group vendor_imeiprov, read-only. Without it the tool, which runs
  without capabilities, could not open the partition.
- Let the camera provider stat `/proc/meminfo` as well as read it: CamX checks
  the file before reading it, and only the stat was denied.
- Load the vendor kernel modules in parallel streams instead of one serial
  `modprobe`: the platform modules first, then one stream per subsystem at the
  same time, each in the kernel list's order and in the same `vendor_modprobe`
  domain, now with only CAP_SYS_MODULE. init no longer waits for the touch
  driver, whose probe takes about a second. Built.
- Add a power stats HAL (`power/stats`, IPowerStats V2), which stock does
  not have. It reports the SoC sleep modes (AOSD, CXSD, DDR) and the modem,
  WPSS, ADSP and CDSP sleep time from the qcom_stats driver's `/dev/stats`
  ioctls, so batterystats, statsd and `dumpsys powerstats` see subsystem
  sleep. No energy meters or consumers. It runs as its own user
  (`vendor_powerstats`) without capabilities in the platform
  `hal_power_stats_default` domain; only it may open `/dev/stats`, and only
  the seven commands it uses. Built.
- Run the stock CamX camera provider under a seccomp filter. The tools
  renderer renames the provider's libhardware.so dependency to our
  libcamxjail.so (`camera/seccomp`), which links libhardware and installs
  the filter before the provider's main(), so it covers CamX from its first
  load. `camera-provider.policy` lists the allowed system calls: threads but
  no child processes, Unix and QRTR sockets only, no writable and executable
  mappings. For now calls outside the list are logged (kernel audit record
  type 1326) and allowed; `seccomp_audit.py` turns those records into policy
  additions. The provider aborts if the filter cannot be installed.
- Replace the CodeLinaro power HAL and Qualcomm's closed perf2 daemon, which
  ran as root, with LineageOS's libperfmgr power HAL (`power/`). It runs as
  system with CAP_SYS_NICE only, writes only the CPU and GPU frequency limits,
  the GPU wake trigger and the tap-to-wake switch that init hands to it, and
  reaches WALT `sched_boost` only through fixed init property triggers. It adds
  touch, launch, rendering and ADPF boosts. A no-op `libqti-perfd-client`
  ends the camera's perf2 lookups. Built.
- Replace Qualcomm's closed tftp_server and pd-mapper with the open-source
  linux-msm tqftpserv (our fork, with upstream's pending memory and path
  fixes, unlink and truncation) and pd-mapper, each under its own user with no
  capabilities and in its own narrow SELinux domain. The modem's read-write
  TFTP files move from persist to `/data/vendor/tmp/tqftpserv`, so a factory
  reset clears them; the `/vendor/rfs` links are gone. Qualcomm's libqrtr.so
  stays for its stock users; pd-mapper loads the linux-msm library as
  libqrtr_linux_msm.so.
- Replace the stock QtiTelephonyService with our own call-audio bridge
  (`callaudio/`): it passes the radio daemon's in-call audio parameters to the
  audio HAL with the normal MODIFY_AUDIO_SETTINGS permission instead of
  MODIFY_AUDIO_ROUTING, in its own SELinux domain.
- Stop setting androidboot.fstab_suffix in the vendor bootconfig; the bootloader
  already appends it, and the duplicate made the kernel reject all bootconfig.
- Build protected VM firmware from source and describe it in the system AVB
  chain, which the FP6 bootloader requires before loading a slot.
- Select the generic first-stage ramdisk, zero GKI header version fields and
  preserve the FP6 boot/recovery AVB chains and separate patch metadata.
- Integrate the FP6 product, source HALs, standard USB setup and development
  boot/recovery layout.
- Maintain selected Qualcomm policy as device source with exact upstream
  provenance and enforcing native checks.
- Install the selected graphics, trusted-execution and Type-C ownership rules.
  Device boot and runtime behavior remain to be verified.
