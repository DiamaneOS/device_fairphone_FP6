# Changelog

## Unreleased

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
