# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Power HAL: LineageOS's maintained libperfmgr power HAL (IPower v7, ADPF),
# built from hardware/lineage/interfaces and hardware/google/pixel (both pinned
# unmodified in the manifest) and driven by powerhint.json. It replaces the
# CodeLinaro power HAL and Qualcomm's closed perf2 daemon, which ran as root:
# this HAL runs as system with CAP_SYS_NICE only (init.fp6.power.rc) in a
# narrow hal_power_default domain (sepolicy/fp6/power.te).
PRODUCT_SOONG_NAMESPACES += \
    hardware/google/interfaces \
    hardware/google/pixel/power-libperfmgr \
    hardware/lineage/interfaces/power-libperfmgr

PRODUCT_PACKAGES += \
    android.hardware.power-service.lineage-libperfmgr \
    libqti-perfd-client

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/power/powerhint.json:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.json \
    device/fairphone/FP6/power/init.fp6.power.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.power.rc

# INTERACTION boosts run on timers (the defaults 1.4 to 5.65 s): the Qualcomm
# display driver has no idle_state node for the HAL to wait on.
PRODUCT_VENDOR_PROPERTIES += vendor.powerhal.disp.idle_support=false

# Let SurfaceFlinger and HWUI open ADPF hint sessions, as on Pixels. Both are
# off otherwise: HWUI defaults to off, and SurfaceFlinger falls back to a
# Google server flag this build does not have.
PRODUCT_VENDOR_PROPERTIES += \
    debug.sf.enable_adpf_cpu_hint=true \
    debug.hwui.use_hint_manager=true
