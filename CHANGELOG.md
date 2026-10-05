# Changelog

## Unreleased

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
