# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Modem subsystem (MSS, 4080000.remoteproc-mss) for private bring-up. The stock
# peripheral manager boots it: pm-proxy votes for the internal modem and
# pm-service starts it through /dev/remoteprocN (boot/ueventd.rc gives the node
# to system, owner-only). pd-mapper, pm-service, pm-proxy, rmt_storage,
# tftp_server, ssr_setup and their QMI/QRTR libraries come from the generated
# vendor tree (component remote-processor-services). The modem firmware is on
# the modem partition (/vendor/firmware_mnt, boot/fstab.qcom).
# Not included: full RAM dump collection (subsystem_ramdump), diagnostics,
# time_daemon and the IPA offload manager (ipacm); see the modem review.
# Crash data: remoteproc coredumps stay disabled (kernel default), and the
# remote processors' error-log path to /data (/vendor/rfs/*/*/ramdumps and
# /data/vendor/tombstones/rfs) is not installed (rfs-links.mk, boot/init.qcom.rc).

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/modem/init.modem.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.modem.rc

# Subsystem restart for the modem and DSPs, as in the stock vendor build.prop.
# ssr_setup turns it into "enabled" in /sys/class/remoteproc/*/recovery; without
# it the kernel panics the phone when a remote processor crashes.
PRODUCT_VENDOR_PROPERTIES += \
    persist.vendor.ssr.restart_level=ALL_ENABLE
