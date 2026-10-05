# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Modem subsystem (MSS, 4080000.remoteproc-mss) for bring-up. The stock
# peripheral manager boots it: pm-proxy votes for the internal modem and
# pm-service starts it through /dev/remoteprocN (boot/ueventd.rc gives the node
# to system, owner-only). pm-service, pm-proxy, rmt_storage, ssr_setup and
# their QMI libraries come from the generated vendor tree (component
# remote-processor-services). The modem firmware is on the modem partition
# (/vendor/firmware_mnt, boot/fstab.qcom).
# Not included: full RAM dump collection (subsystem_ramdump), diagnostics,
# time_daemon and the IPA offload manager (ipacm).
# Crash data: remoteproc coredumps stay disabled (kernel default), and the
# remote processors' error logs are not written to /data: tqftpserv serves no
# ramdump paths and there is no /data/vendor/tombstones/rfs (boot/init.qcom.rc).

# Open-source remote-processor services from linux-msm (BSD-3-Clause, pinned in
# the manifest) in place of Qualcomm's closed ones:
# - tqftpserv (vendor/qcom/opensource/tqftpserv, DiamaneOS fork with memory
#   and path fixes, unlink and truncation) replaces tftp_server. It serves the
#   modem's TFTP requests over QRTR: read-only files from the modem partition
#   through /vendor/firmware/modem_pr (Android.bp) and read-write files in
#   /data/vendor/tmp/tqftpserv (init.modem.rc), not persist.
# - pd-mapper (vendor/qcom/opensource/pd-mapper, unmodified) replaces
#   Qualcomm's pd-mapper. It serves the protection-domain lists (*.jsn, linked
#   into /vendor/firmware by Android.bp) to the ADSP, CDSP, modem and audio HAL.
# Both use the linux-msm libqrtr (vendor/qcom/opensource/qrtr): tqftpserv links
# it statically, pd-mapper loads it as libqrtr_linux_msm.so. Qualcomm's
# libqrtr.so stays for the stock display, VM and QTEE libraries that link it.
# Each runs as its own user without capabilities (config.fs); init and bionic
# look those users up in the generated vendor passwd and group files.
PRODUCT_PACKAGES += \
    passwd_vendor \
    group_vendor \
    tqftpserv \
    pd-mapper \
    fp6_firmware_modem_pr \
    fp6_firmware_adspr_jsn \
    fp6_firmware_adsps_jsn \
    fp6_firmware_adspua_jsn \
    fp6_firmware_battmgr_jsn \
    fp6_firmware_cdspr_jsn \
    fp6_firmware_modemr_jsn

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/modem/init.modem.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.modem.rc

# Subsystem restart for the modem and DSPs, as in the stock vendor build.prop.
# ssr_setup turns it into "enabled" in /sys/class/remoteproc/*/recovery; without
# it the kernel panics the phone when a remote processor crashes.
PRODUCT_VENDOR_PROPERTIES += \
    persist.vendor.ssr.restart_level=ALL_ENABLE
