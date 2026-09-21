# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

PRODUCT_SHIPPING_API_LEVEL := 35
PRODUCT_USE_DYNAMIC_PARTITIONS := true
PRODUCT_BUILD_SUPER_PARTITION := true
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/launch_with_vendor_ramdisk.mk)

# Keep one reviewed UFS fstab for early and late mounting.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/boot/fstab.qcom:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.qcom \
    device/fairphone/FP6/boot/fstab.qcom:$(TARGET_COPY_OUT_VENDOR_RAMDISK)/first_stage_ramdisk/system/etc/fstab.qcom

# The generated selection owns HAL packages, dependencies, properties and
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
