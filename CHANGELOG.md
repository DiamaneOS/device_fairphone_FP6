# Changelog

## Unreleased

- Build the GNSS HAL, its IGnss implementation and the six location libraries
  from CodeLinaro source (`hardware/qcom/gps`, `vendor/qcom/opensource/location`,
  `core-utils`) instead of Qualcomm's closed builds. The QMI client is the
  public QMI framework, linked into `libloc_api_v02`, so the GNSS process
  loads no closed code. As in Qualcomm's builds: CFI on all of them, the
  integer overflow sanitizer on the HAL and the QMI LOC client.
  - The libraries load only `libgnss` and `libloc_api_v02`: Qualcomm's IZat,
    XTRA, NTRIP and other add-on libraries cannot load.
  - The HAL opens no socket for the XTRA and DGNSS daemons, so
    `/dev/socket/location` and the HAL's socket rules are gone.
  - The stock `gps.conf`, `izat.conf` (pinned edits) and `sap.conf` stay;
    SUPL and PSDS settings work as before. Not yet built.
- Use the factory Bluetooth address. imeiprovd now reads it from the
  traceability partition at boot and sets
  `ro.vendor.diamaneos.bt.factory_address`; `init.fp6.bluetooth.rc` copies
  it to `ro.vendor.bt.boot.macaddr`, as stock's `tctd.rc` does, which the HCI
  implementation reads before falling back to its stored generated address
  (prefix 22:22). The HAL property gets its own type, read only by the HAL
  and set only by vendor_init. Existing pairings may need pairing again once.
  Not yet built.
- Compress zram with lz4 instead of zstd. Measured over two 20-app relaunch
  rounds each: relaunch median 137 -> 118 ms, memory stall 0.78 -> 0.47 s,
  same kills; costs a lower compression ratio (about 4 instead of 6) and a
  little more kswapd CPU.
- The source-built gralloc no longer tries to load `libubwcp.so` when UBWC-P
  support is compiled out (`hardware/qcom/display`), so apps, system_server,
  SurfaceFlinger and the media services no longer log its denial; removed
  from the expected-denials list. The camera provider still loads it itself.
  Not yet built.
- Give the switch that turns Qualcomm's embedded USB debugger (EUD) on,
  `/sys/module/eud/parameters/enable`, its own SELinux type with a
  neverallow: only ueventd and vendor_init keep write access (the platform
  grants them all sysfs types); vold, the USB HAL and vfio_handler lose it.
  The debugger stays off. Not yet built.
- Drop stock's `wowlan_triggers=magic_pkt` from the station supplicant
  overlay. The magic-packet wake-up itself is turned off in the driver
  configuration that the tools generate (`gEnableWoW=2`). Not yet built.
- Remove the imported hwservice contexts of the ANT, ANT HCI and Bluetooth
  SAR interfaces, whose libraries are not shipped: the Bluetooth HAL could
  still register them under its own label. Not yet built.
- Run the boot control HAL as its own user, `vendor_bootctl`, with
  CAP_SYS_RAWIO only (stock: root with every capability). ueventd gives the
  group the GPT disks of the A/B LUNs (sdb, sdc, sde), misc and the UFS BSG
  node; the other LUNs stay root-only. Needs the matching
  `hardware/qcom/bootctrl` change. Not yet built.
- Build the dm-verity hash trees of system, system_ext, product, vendor,
  odm, vendor_dlkm and system_dlkm with SHA-256 instead of avbtool's SHA-1
  default, as stock does. Not yet built; needs a super flash.
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
  installed. Not yet built.
- Add DiamaneOS's eSIM manager (DiamaneOSEuicc), which manages the profiles on
  the eUICC: list, turn on and off, rename, delete. It ships disabled; the eSIM
  support switch in Settings turns it on and restarts the phone. Mark physical
  slot 1 as a built-in eUICC (`non_removable_euicc_slots`), which stock leaves
  unset. Not yet built.
- Use AOSP's software audio effects only: our own `audio_effects.xml` lists
  the AOSP bundle, reverb, visualizer, downmix, loudness and dynamics
  effects without DSP offload halves or the effect proxy, so apps' effect
  parameters no longer reach Qualcomm's closed offload bundle and
  visualizer, which are no longer shipped. Qualcomm's VoIP AEC/NS
  descriptors (the DSP's echo cancellation) and volume listener are built
  from unmodified CodeLinaro audio-ar sources (`audio/effects`) under
  their stock names. Not yet built.
- Lower the GPU floor for EXPENSIVE_RENDERING from 763 to 510 MHz: GPU
  composition rarely needs the pin, the GPU governor still clocks up when
  busy, and 510 MHz runs at a lower voltage. Not yet built.
- Let ART compile on the phone for the actual cores (runtime CPU variant
  cortex-a55: LSE atomics, FP16, dot product, no Cortex-A53 workarounds), as
  on Pixel 9, which also has A720 and A520 cores. Not yet built.
- Raise the DDR and L3 floors for up to 3 s during app launches, as stock's
  perf daemon did: launches start memory-bound. Not yet built.
- Keep the stock Bluetooth HCI implementation's two firmware-download tags at
  warning level: at info level they logged the Bluetooth address each time
  Bluetooth started, and at debug level HCI command dumps. Not yet built.
- Add neverallows that keep every domain except init, vendor_init and
  vold_prepare_subdirs from creating or relabelling files and directories to
  the fingerprint HAL's data label, so no other process can plant a
  configuration the closed fingerprint module would read. No rule changes.
- Remove the imported policy of the display colour service, which is not
  installed or declared: its domain, executable label and service contexts,
  and SystemUI's client grant to it. Not yet built.
- Drop the imported secure-processor (SPU) grants of the gatekeeper HAL and
  qseecomd: the FP6 has no SPU, its drivers are not shipped and the selected
  gatekeeper names no SPU node, so those device nodes never exist. Not yet
  built.
- Make the UFS storage's serial number and its LUNs' SCSI serial and
  identification pages root-only through ueventd. No shipped program reads
  them, and their generic sysfs label is readable by 22 system and vendor
  domains, among them the fingerprint HAL and the composer. Not yet built.
- Document why four denials stay denied (the audio HAL's kernel wake locks,
  which only sound trigger takes; SystemUI's read of an LE audio property
  that is not set; the IWLAN and call-audio apps' start-up lookups of the GPU
  and network statistics services) and why nicmd needs to read XFRM state:
  it removes the Wi-Fi calling security associations it installs by dumping
  all of them.
- Run the thermal HAL as system without capabilities instead of root, as the
  Pixel thermal HAL runs. ueventd gives group system the two trip nodes it
  writes; they stay root-owned for the thermal engine. Not yet built.
- Give the SoC serial number its own SELinux type and make it root-only. No
  shipped program reads it; the composer's read of all sysfs now excludes it,
  the thermal engine no longer reads the soc0 files beyond the public ids,
  and a neverallow keeps vendor services off it. The platform still lets some
  HAL domains read all sysfs, but those run as their own users. Not yet built.
- Raise the ADPF uclamp ceiling from 384 to 512: 384 is below the capacity
  of the A520 little cores (about 454), so a hint session running over its
  target, SurfaceFlinger included, could never move to a bigger core. Normal
  frames keep the lower starting values. Not yet built.
- Remove the camera streaming modes' little-core caps from
  `powerhint.json`: CamX runs on the little cores, so the caps would slow it,
  and nothing sends those modes. CAMERA_SHOT now also sets `sched_boost`, like
  CAMERA_LAUNCH, so capture work can leave the little cores. Not yet built.
- Pass the stock camera's performance hints to the power HAL. CamX's open,
  close and snapshot hints become CAMERA_LAUNCH and CAMERA_SHOT boosts of at
  most 5 s (a hint held until release at most 2 s), ended early when CamX
  releases them; `libqti-perfd-client` sends them from its own thread with
  one-way calls, so the camera never waits for the power HAL. The camera
  provider becomes a power HAL client, as on Pixels. Not yet built.
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
  driver, whose probe takes about a second. Not yet built.
- Add a power stats HAL (`power/stats`, IPowerStats V2), which stock does
  not have. It reports the SoC sleep modes (AOSD, CXSD, DDR) and the modem,
  WPSS, ADSP and CDSP sleep time from the qcom_stats driver's `/dev/stats`
  ioctls, so batterystats, statsd and `dumpsys powerstats` see subsystem
  sleep. No energy meters or consumers. It runs as its own user
  (`vendor_powerstats`) without capabilities in the platform
  `hal_power_stats_default` domain; only it may open `/dev/stats`, and only
  the seven commands it uses. Not yet built.
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
  ends the camera's perf2 lookups. Not yet built.
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
