# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project
# Recovery package selection: Copyright (C) 2018 The Android Open Source Project

PRODUCT_SHIPPING_API_LEVEL := 35
# The matched Fairphone 6.1 kernel uses 4 KiB pages. Validate every selected
# prebuilt against this size rather than the generic ARM64 16 KiB default.
PRODUCT_MAX_PAGE_SIZE_SUPPORTED := 4096
PRODUCT_CHECK_PREBUILT_MAX_PAGE_SIZE := true
PRODUCT_USE_DYNAMIC_PARTITIONS := true
PRODUCT_BUILD_SUPER_PARTITION := true
PRODUCT_BUILD_RECOVERY_IMAGE := true
# Userdata is formatted through recovery, never packaged from a reference image.
PRODUCT_BUILD_USERDATA_IMAGE := false
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/launch_with_vendor_ramdisk.mk)

# The stock GPU driver and graphics mapper are HIDL HALs. Android only includes
# hwservicemanager by default for devices shipping at API 34 or older, and
# without it HIDL, including passthrough HALs, is unavailable.
PRODUCT_PACKAGES += hwservicemanager

# The stock display composer registers its QService on /dev/vndbinder.
# Android only includes vndservicemanager for devices shipping at API 29 or
# older, and this product does not inherit base_vendor.mk.
PRODUCT_PACKAGES += vndservicemanager

# Diagnostic (non-user) builds only: trust a developer adb key kept outside
# version control, so adb works before the setup UI exists. Inert for user builds.
ifneq ($(TARGET_BUILD_VARIANT),user)
  ifneq ($(wildcard vendor/diamaneos-diag/adb_keys),)
    PRODUCT_ADB_KEYS := vendor/diamaneos-diag/adb_keys
  endif
endif

# Generate the standard device compatibility matrix and system SDK requirements.
# generic_system does not inherit the base_vendor package selection.
PRODUCT_PACKAGES += vendor_compatibility_matrix.xml

# Select the platform recovery runtime explicitly: image generation alone does
# not select these packages when generic_system is used without base_vendor.
# This follows base_vendor.mk's recovery group; fastbootd serves dynamic partitions.
PRODUCT_PACKAGES += \
    adbd.recovery \
    cgroups.recovery.json \
    charger.recovery \
    fastbootd \
    init_second_stage.recovery \
    ld.config.recovery.txt \
    linker.recovery \
    otacerts.recovery \
    recovery \
    servicemanager.recovery \
    shell_and_utilities_recovery \
    watchdogd.recovery

PRODUCT_VENDOR_PROPERTIES += \
    ro.recovery.usb.vid=18D1 \
    ro.recovery.usb.adb.pid=D001 \
    ro.recovery.usb.fastboot.pid=4EE0

# Recovery has no zygote or normal vendor init; platform recovery imports this.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/boot/init.recovery.qcom.rc:$(TARGET_COPY_OUT_RECOVERY)/root/init.recovery.qcom.rc

# Keep one reviewed UFS fstab for early and late mounting.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/boot/ueventd.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.rc \
    device/fairphone/FP6/boot/init.qcom.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.qcom.usb.rc \
    device/fairphone/FP6/boot/init.qcom.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.qcom.rc \
    device/fairphone/FP6/boot/fstab.qcom:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.qcom \
    device/fairphone/FP6/boot/fstab.qcom:$(TARGET_COPY_OUT_VENDOR_RAMDISK)/first_stage_ramdisk/system/etc/fstab.qcom

# Firmware, DSP and Bluetooth mount points from fstab.qcom (see Android.mk).
# Without them the modem partition is never mounted and the ADSP, CDSP and
# modem cannot load their firmware.
PRODUCT_PACKAGES += \
    fp6_vendor_mount_point_firmware_mnt \
    fp6_vendor_mount_point_dsp \
    fp6_vendor_mount_point_bt_firmware
$(call inherit-product, device/fairphone/FP6/rfs-links.mk)

# Source-built hardware services. Boot control owns GPT slot attributes and
# UFS selection in both normal Android and recovery.
PRODUCT_PACKAGES += \
    toolbox_vendor \
    android.hardware.boot-service.qti \
    android.hardware.boot-service.qti.recovery \
    android.hardware.power-service \
    android.hardware.thermal-service.qti \
    vendor.qti.hardware.lights.service \
    vendor.qti.hardware.vibrator.service \
    android.hardware.usb-service.qti \
    android.hardware.usb.gadget-service.qti \
    usb_compositions.conf \
    android.hardware.health-service.qti \
    android.hardware.health-service.qti_recovery

# Placeholder audio: the AOSP reference HAL with stub streams, so audioserver
# and the framework audio service can start. Replace with the Qualcomm stack.
PRODUCT_PACKAGES += com.android.hardware.audio
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/audio/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/bluetooth_with_le_audio_policy_configuration_7_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_with_le_audio_policy_configuration_7_0.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml
$(call inherit-product, hardware/interfaces/audio/aidl/default/audio_effects.mk)

# Phone memory profile and graphics version, matching stock. Without the heap
# properties ART falls back to a 16 MB heap and the framework runs out of memory.
$(call inherit-product, frameworks/native/build/phone-xhdpi-6144-dalvik-heap.mk)
PRODUCT_VENDOR_PROPERTIES += ro.opengles.version=196610

# Display composition settings the selected Qualcomm composer and SurfaceFlinger
# expect (identical to the stock vendor build.prop).
PRODUCT_VENDOR_PROPERTIES += \
    ro.surface_flinger.use_color_management=true \
    ro.surface_flinger.protected_contents=true \
    ro.surface_flinger.use_content_detection_for_refresh_rate=true \
    ro.surface_flinger.set_touch_timer_ms=3500000 \
    ro.surface_flinger.set_idle_timer_ms=3500 \
    ro.surface_flinger.force_hwc_copy_for_virtual_displays=true \
    ro.surface_flinger.max_frame_buffer_acquired_buffers=3 \
    ro.surface_flinger.max_virtual_display_dimension=4096 \
    ro.surface_flinger.clear_slots_with_set_layer_buffer=false \
    ro.surface_flinger.supports_background_blur=0 \
    ro.surface_flinger.has_wide_color_display=true \
    ro.surface_flinger.has_HDR_display=true \
    ro.surface_flinger.wcg_composition_dataspace=143261696 \
    persist.sys.sf.color_saturation=1.0 \
    persist.sys.sf.color_mode=9 \
    debug.sf.hw=0 \
    debug.sf.latch_unsignaled=1 \
    debug.sf.auto_latch_unsignaled=1 \
    debug.sf.disable_client_composition_cache=0 \
    debug.sf.enable_gl_backpressure=1 \
    debug.sf.enable_advanced_sf_phase_offset=1 \
    debug.sf.use_phase_offsets_as_durations=1 \
    debug.sf.late.app.duration=13666666 \
    debug.sf.early.app.duration=13666666 \
    debug.sf.earlyGl.app.duration=13666666 \
    debug.sf.early.sf.duration=10500000 \
    debug.sf.earlyGl.sf.duration=10500000 \
    debug.sf.late.sf.duration=10500000 \
    debug.sf.predict_hwc_composition_strategy=0 \
    debug.sf.treat_170m_as_sRGB=1 \
    vendor.gralloc.disable_ubwc=0 \
    vendor.display.disable_scaler=0 \
    vendor.display.disable_excl_rect=0 \
    vendor.display.disable_excl_rect_partial_fb=1 \
    vendor.display.comp_mask=0 \
    vendor.display.enable_optimize_refresh=0 \
    vendor.display.use_smooth_motion=1 \
    vendor.display.disable_stc_dimming=1 \
    vendor.display.enable_dpps_dynamic_fps=1 \
    vendor.display.vds_allow_hwc=1 \
    vendor.display.enable_async_vds_creation=1 \
    vendor.display.enable_rounded_corner=1 \
    vendor.display.disable_3d_adaptive_tm=1 \
    vendor.display.disable_sdr_dimming=0 \
    vendor.display.enable_rc_support=1 \
    vendor.display.disable_sdr_histogram=1 \
    vendor.display.enable_hdr10_gpu_target=1 \
    vendor.display.enable_display_extensions=1 \
    vendor.display.disable_offline_rotator=1 \
    vendor.display.enable_async_powermode=0 \
    vendor.display.disable_hw_recovery_dump=1 \
    vendor.display.enable_early_wakeup=1

# Declare the phone baseline and only the hardware that currently works; add
# radio, camera, sensor and other features as their stacks are brought up.
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/handheld_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level-1.xml \
    frameworks/native/data/etc/android.hardware.vulkan.compute-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.compute-0.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_3.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version-1_3.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml

# Match the trusted-execution backend selected by the authenticated stock image.
PRODUCT_VENDOR_PROPERTIES += vendor.gatekeeper.is_security_level_spu=0

# The generated selection owns remaining HAL packages, dependencies, properties and
# notices. Absence is an error: do not silently omit required hardware inputs.
$(call inherit-product, vendor/fairphone/FP6/device-vendor.mk)
# Kernel artifacts are produced from the independently pinned kernel workspace.
$(call inherit-product, device/fairphone/FP6-kernel/device-kernel.mk)

# Source-built protected VM firmware for the FP6 pvmfw partitions.
PRODUCT_BUILD_PVMFW_IMAGE := true

AB_OTA_UPDATER := true
AB_OTA_PARTITIONS += \
    boot \
    dtbo \
    init_boot \
    odm \
    product \
    pvmfw \
    recovery \
    system \
    system_dlkm \
    system_ext \
    vbmeta \
    vbmeta_system \
    vendor \
    vendor_boot \
    vendor_dlkm

# FP6 controller and the native gadget service, with FunctionFS MTP/PTP.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.usb.controller=a600000.dwc3 \
    vendor.usb.use_ffs_mtp=1 \
    vendor.usb.use_gadget_hal=1 \
    vendor.usb.rndis.func.name=rndis \
    vendor.usb.ncm.func.name=ncm
