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

# Power stats HAL (power/stats): SoC and remote-processor sleep residency from
# the qcom_stats driver for batterystats, statsd and dumpsys powerstats. It
# runs as its own user (config.fs) without capabilities and opens only
# /dev/stats. Stock FP6 ships no power stats HAL.
PRODUCT_PACKAGES += android.hardware.power.stats-service.fp6

# powerhint.json has no camera streaming modes (Mode::CAMERA_STREAMING_*).
# Pixels cap the CPUs in them and move the camera daemon to bigger cores;
# Qualcomm's own streaming tunings cap the little cores and lower the migration
# thresholds. This HAL can make neither placement change, and CamX runs on the
# little cores, so a cap alone would slow it. Nothing sends the modes, and
# libqti-perfd-client does not forward CamX's streaming hints.
#
# LAUNCH also raises the memory-bus floors stock's perf daemon raised for app
# launches, for at most 3 s: DDR (bandwidth monitor, request 800000 kHz) and L3
# (prime latency monitor, request 1344000 kHz). The bus driver rounds a request
# up to the next level of its table; on LPDDR5 (547, 768, 1555, ... MHz) the
# DDR floor is 1555 MHz. "setprop vendor.powerhal.membus.enable false" turns
# the launch floors off for A/B tests. INTERACTION raises the DDR floor of
# stock's scroll boost (request 681000 kHz, 768 MHz on LPDDR5) while the HAL
# holds the hint, 1.4 to 5.65 s after a touch; its switch is
# vendor.powerhal.interaction_ddr.enable.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/power/powerhint.json:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.json \
    device/fairphone/FP6/power/init.fp6.power.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.power.rc

# The thermal HAL (device.mk) runs as system instead of root
# (init.fp6.thermal.rc); ueventd gives it the two trip nodes it writes.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/power/init.fp6.thermal.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.thermal.rc

# SurfaceFlinger's main and RenderEngine threads join the top-app cpuset, as on
# Pixels. AOSP's default (and stock's) is system-background, which this device
# limits to the silver cores (boot/init.fp6.perf.rc), so SF could never use a
# bigger core and its ADPF boosts had no effect.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/power/task_profiles.json:$(TARGET_COPY_OUT_VENDOR)/etc/task_profiles.json

# INTERACTION boosts run on timers (the defaults 1.4 to 5.65 s): the Qualcomm
# display driver has no idle_state node for the HAL to wait on.
PRODUCT_VENDOR_PROPERTIES += vendor.powerhal.disp.idle_support=false

# Let SurfaceFlinger and HWUI open ADPF hint sessions, as on Pixels. Both are
# off otherwise: HWUI defaults to off, and SurfaceFlinger falls back to a
# Google server flag this build does not have.
PRODUCT_VENDOR_PROPERTIES += \
    debug.sf.enable_adpf_cpu_hint=true \
    debug.hwui.use_hint_manager=true
