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

# Match the trusted-execution backend selected by the authenticated stock image.
PRODUCT_VENDOR_PROPERTIES += vendor.gatekeeper.is_security_level_spu=0

# The generated selection owns remaining HAL packages, dependencies, properties and
# notices. Absence is an error: do not silently omit required hardware inputs.
$(call inherit-product, vendor/fairphone/FP6/device-vendor.mk)
# Kernel artifacts are produced from the independently pinned kernel workspace.
$(call inherit-product, device/fairphone/FP6-kernel/device-kernel.mk)

AB_OTA_UPDATER := true
AB_OTA_PARTITIONS += \
    boot \
    dtbo \
    init_boot \
    odm \
    product \
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
