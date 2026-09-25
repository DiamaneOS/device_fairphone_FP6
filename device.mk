# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

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

# Qualcomm display stack built from source (hardware/qcom-caf/sm8650/display and
# its interface repositories; OP-DISPLAY-HAL-SOURCE). The stock Android 14
# composer cannot present under Android 17. Configuration values follow
# LineageOS for 6.1-kernel platforms; stock declares a wide-colour panel.
$(call soong_config_set,qtidisplay,default,true)
$(call soong_config_set,qtidisplay,drmpp,true)
$(call soong_config_set,qtidisplay,gralloc4,true)
$(call soong_config_set,qtidisplay,headless,false)
$(call soong_config_set,qtidisplay,llvmcov,false)
$(call soong_config_set,qtidisplay,llvmsa,false)
$(call soong_config_set,qtidisplay,smmu_proxy,false)
$(call soong_config_set,qtidisplay,ubwcp_headers,false)
$(call soong_config_set,qtidisplay,udfps,false)
$(call soong_config_set,qtidisplay,var1,false)
$(call soong_config_set,qtidisplay,var2,false)
$(call soong_config_set,qtidisplay,var3,false)
$(call soong_config_set,qtidisplay,wide_color,true)
PRODUCT_PACKAGES += \
    android.hardware.graphics.mapper@4.0-impl-qti-display \
    vendor.qti.hardware.display.allocator-service \
    vendor.qti.hardware.display.composer-service \
    vendor.qti.hardware.display.composer-service.rc \
    vendor.qti.hardware.display.composer-service.xml \
    vendor.qti.hardware.display.mapper@4.0.vendor \
    vendor.display.config@2.0.vendor \
    libdisplayconfig.qti \
    libdrmutils \
    libfilefinder \
    libgpu_tonemapper \
    libgralloc.qti \
    libqdMetaData \
    libqdutils \
    libsdedrm \
    libsdmcore \
    libsdmdal \
    libsdmutils

# tinyxml2 with the Android 14 ABI for the stock display color manager (see
# compat/tinyxml2-v34).
PRODUCT_PACKAGES += libtxml2v34

# Source-built Android 17 Wi-Fi services. The pinned kernel loads its QCA6750
# driver; the vendor HAL is Qualcomm's CodeLinaro Wi-Fi HAL (hardware/qcom/wlan
# fork, Soong-converted). The HAL signals driver readiness through /dev/wlan
# (BoardConfig.mk). Station mode only: hostapd (hotspot) is deferred until
# station mode is validated, and the stock cnss-daemon is not selected.
PRODUCT_SOONG_NAMESPACES += \
    hardware/qcom/wlan/cld80211-lib \
    hardware/qcom/wlan/qcwcn
$(call inherit-product, vendor/diamaneos/config/wifi.mk)
PRODUCT_PACKAGES += \
    libcld80211 \
    wpa_supplicant.conf \
    fp6_wlan_cfg_ini \
    fp6_wlan_mac_bin
# Supplicant overlays, identical to stock. p2p_disabled keeps P2P off wlan0;
# otherwise the supplicant registers wlan0 as its P2P interface and the
# framework cannot add it as a station.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/wifi/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf \
    device/fairphone/FP6/wifi/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf

# Audio: the stock AudioReach userspace with AOSP source adapters (audio/audio.mk).
$(call inherit-product, device/fairphone/FP6/audio/audio.mk)

# r9p hardware bring-up (reviews/OP-HW-BRINGUP-2026-09-25). Each subsystem
# makefile documents its stock and source-built parts.
$(call inherit-product, device/fairphone/FP6/modem/modem.mk)
$(call inherit-product, device/fairphone/FP6/bluetooth/bluetooth.mk)
$(call inherit-product, device/fairphone/FP6/nfc/nfc.mk)
$(call inherit-product, device/fairphone/FP6/gnss/gnss.mk)

# Phone memory profile and graphics version, matching stock. Without the heap
# properties ART falls back to a 16 MB heap and the framework runs out of memory.
$(call inherit-product, frameworks/native/build/phone-xhdpi-6144-dalvik-heap.mk)
PRODUCT_VENDOR_PROPERTIES += ro.opengles.version=196610

# The FP6 panel is 1116x2484 at 480 dpi on stock Android 16. Without an
# explicit density, Android 17 chooses 213 dpi and renders the UI too small.
# A panel property, so it belongs to the vendor partition.
PRODUCT_VENDOR_PROPERTIES += ro.sf.lcd_density=480

# Framework, Settings and SystemUI hardware configuration (rro/): display
# cutout, corners, status bar, 120 Hz refresh rate, the Smooth display switch
# and the side fingerprint sensor description.
PRODUCT_PACKAGES += \
    FP6FrameworksOverlay \
    FP6SettingsOverlay \
    FP6SystemUIOverlay

# Fingerprint: our AIDL service (fingerprint/) drives the stock FocalTech
# module. The module keeps its own SONAME as file name, so it cannot collide
# with AOSP's reference fingerprint modules; hw_get_module finds it through
# ro.hardware.fingerprint=fp6 and the hw/fingerprint.fp6.so link.
PRODUCT_VENDOR_PROPERTIES += ro.hardware.fingerprint=fp6
PRODUCT_PACKAGES += android.hardware.biometrics.fingerprint-service.fp6

# Display composition settings the selected Qualcomm composer and SurfaceFlinger
# expect, from the stock vendor build.prop, with two exceptions:
# - no touch timer override: stock's 3500000 ms kept touch boost active for
#   about 58 minutes and blocked idle refresh-rate drops; SurfaceFlinger's
#   default (200 ms, as in Qualcomm's display config) applies instead;
# - debug.sf.hw, debug.sf.latch_unsignaled and
#   debug.sf.enable_advanced_sf_phase_offset are left out: nothing in this
#   platform, the display HAL or the stock vendor files reads them
#   (unsignaled latching is debug.sf.auto_latch_unsignaled).
PRODUCT_VENDOR_PROPERTIES += \
    ro.surface_flinger.use_color_management=true \
    ro.surface_flinger.protected_contents=true \
    ro.surface_flinger.use_content_detection_for_refresh_rate=true \
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
    debug.sf.auto_latch_unsignaled=1 \
    debug.sf.disable_client_composition_cache=0 \
    debug.sf.enable_gl_backpressure=1 \
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
# radio, camera and other features as their stacks are brought up.
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/handheld_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level-1.xml \
    frameworks/native/data/etc/android.hardware.vulkan.compute-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.compute-0.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_3.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version-1_3.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml

# Sensors: the AOSP multi-HAL loads the sub-HALs in /vendor/etc/sensors/hals.conf
# (the Qualcomm sub-HAL comes with the generated vendor tree).
PRODUCT_PACKAGES += \
    android.hardware.sensors-service.multihal \
    sensors.dynamic_sensor_hal

# Sensors served by the ADSP through the sensors multi-HAL (the stock set).
PRODUCT_COPY_FILES += \
    $(foreach f,accelerometer barometer compass dynamic.head_tracker gyroscope light proximity stepcounter stepdetector, \
        frameworks/native/data/etc/android.hardware.sensor.$(f).xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.$(f).xml)

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
