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
# Compressed Virtual A/B, as stock and Pixels: OTAs write lz4-compressed
# snapshots that the userspace snapshot daemon merges (the kernel has dm-user
# and io_uring), so an update needs far less free space on /data. The generic
# ramdisk already has the first-stage snapuserd. Recovery, which also runs
# fastbootd, needs its own to finish a pending merge before a wipe or a flash.
# Takes effect from the first OTA whose source build carries these properties.
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/vabc_features.mk)
PRODUCT_VIRTUAL_AB_COMPRESSION_METHOD := lz4
PRODUCT_VENDOR_PROPERTIES += ro.virtual_ab.compression.threads=true
PRODUCT_PACKAGES += snapuserd.recovery

# The display composer (built from source) registers its QService, and the
# stock peripheral manager its service, on /dev/vndbinder. Android only includes
# vndservicemanager for devices shipping at API 29 or older, and this product
# does not inherit base_vendor.mk.
# Remote key provisioning, as stock: without a hostname keystore2 treats it as
# off, and stock declares the TEE KeyMint RKP-only (no factory attestation
# key). The hostname only switches it on. The server rkpd talks to is set by
# GrapheneOS's RemoteKeyProvisioningSettings (frameworks/base,
# android.ext.settings, default the GrapheneOS proxy, https://<hostname>/v1
# only if the user picks the standard server). DiamaneOS's backend replaces it
# there (tools endpoint contract rkp-proxy).
PRODUCT_VENDOR_PROPERTIES += \
    remote_provisioning.hostname=remoteprovisioning.googleapis.com \
    remote_provisioning.tee.rkp_only=true

# Keys, as stock: when the metadata key is created after a wipe, vold first asks
# KeyMint to delete every old key, rollback-resistant ones included.
# AndroidKeyStore offers the TEE's 3DES keys.
PRODUCT_VENDOR_PROPERTIES += \
    ro.crypto.metadata_init_delete_all_keys.enabled=true \
    ro.hardware.keystore_desede=true

# Device-ID attestation sends these to KeyMint, which checks them against the
# IDs provisioned at the factory, so they carry stock's values. The visible
# brand stays DiamaneOS. Stock sets brand, name and model. Device and
# manufacturer pin what ro.product.vendor.* reports.
PRODUCT_BRAND_FOR_ATTESTATION := Fairphone
PRODUCT_NAME_FOR_ATTESTATION := FP6
PRODUCT_MODEL_FOR_ATTESTATION := Fairphone 6
PRODUCT_DEVICE_FOR_ATTESTATION := FP6
PRODUCT_MANUFACTURER_FOR_ATTESTATION := Fairphone

# Recovery has no zygote or normal vendor init. Platform recovery imports this.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/boot/init.recovery.qcom.rc:$(TARGET_COPY_OUT_RECOVERY)/root/init.recovery.qcom.rc

# Keep one UFS fstab for early and late mounting.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/boot/ueventd.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.rc \
    device/fairphone/FP6/boot/init.qcom.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.qcom.rc \
    device/fairphone/FP6/boot/fstab.qcom:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.qcom \
    device/fairphone/FP6/boot/fstab.qcom:$(TARGET_COPY_OUT_VENDOR_RAMDISK)/first_stage_ramdisk/system/etc/fstab.qcom

# The vendor_dlkm load list split into streams that init.qcom.rc loads in
# parallel. Together they must list every module of the kernel's
# modules.load once, each in its order (image verification checks this).
PRODUCT_COPY_FILES += \
    $(foreach s,platform wireless audio data crypto media display touch, \
        device/fairphone/FP6/boot/modules/modules.load.$(s):$(TARGET_COPY_OUT_VENDOR_DLKM)/lib/modules/modules.load.$(s))

# Firmware, DSP and Bluetooth mount points from fstab.qcom (see Android.mk).
# Without them the modem partition is never mounted and the ADSP, CDSP and
# modem cannot load their firmware.
PRODUCT_PACKAGES += \
    fp6_vendor_mount_point_firmware_mnt \
    fp6_vendor_mount_point_dsp \
    fp6_vendor_mount_point_bt_firmware

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

# composer failed to present under Android 17 when tried without
# libsdmextension and is untested with it; source is kept so we can patch and
# harden the code that handles every app's buffers. Configuration values follow
$(call inherit-product, vendor/diamaneos/config/wifi.mk)
# Hardware subsystems. Each subsystem makefile documents its stock and
# source-built parts.
$(call inherit-product, device/fairphone/FP6/modem/modem.mk)

# Phone memory profile and graphics version, matching stock. Without the heap
# properties ART falls back to a 16 MB heap and the framework runs out of memory.
$(call inherit-product, frameworks/native/build/phone-xhdpi-6144-dalvik-heap.mk)
PRODUCT_VENDOR_PROPERTIES += ro.opengles.version=196610

# The SoC maker and model apps read through Build.SOC_MANUFACTURER and
# Build.SOC_MODEL, as stock (which sets the model at boot from the SoC id,
# both FP6 SoC ids are SM7635).
PRODUCT_VENDOR_PROPERTIES += \
    ro.soc.manufacturer=QTI \
    ro.soc.model=SM7635

# Stock's two ANGLE workarounds for this GPU, used where ANGLE renders
# (Vanadium, WebView, per-app ANGLE): linear filtering for YUV video textures
# and pass-through for an unspecified colour space.
PRODUCT_VENDOR_PROPERTIES += \
    debug.angle.feature_overrides_enabled=preferLinearFilterForYUV:mapUnspecifiedColorSpaceToPassThrough

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

# As stock: filter magnetometer samples (the sub-HAL's default is off), and do
# not offer sensor direct channels, which stock never enabled (the sub-HAL's
# default is on). vendor_init may set them (sepolicy/fp6/platform.te).
PRODUCT_VENDOR_PROPERTIES += \
    persist.vendor.sensors.enable.mag_filter=true \
    persist.vendor.sensors.support_direct_channel=false

# Match the trusted-execution backend selected by the authenticated stock image.
PRODUCT_VENDOR_PROPERTIES += vendor.gatekeeper.is_security_level_spu=0

# The generated selection owns remaining HAL packages, dependencies, properties and
# notices. Absence is an error: do not silently omit required hardware inputs.
$(call inherit-product, vendor/fairphone/FP6/device-vendor.mk)
# Kernel artifacts are produced from the independently pinned kernel workspace.
$(call inherit-product, device/fairphone/FP6-kernel/device-kernel.mk)
# The kernel's GPL-2.0 text and source location, in the vendor licence list.
PRODUCT_PACKAGES += fp6_kernel_notice

# Source-built protected VM firmware for the FP6 pvmfw partitions.
PRODUCT_BUILD_PVMFW_IMAGE := true

# Recovery applies update packages from adb sideload with this tool.
PRODUCT_PACKAGES += update_engine_sideload

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

# MTP cancels pending transfers in one batch, as stock. On the DWC3 controller,
# cancelling them one by one fails after the first, which by the code aborts
# PC-to-phone copies of 4 GB or more.
PRODUCT_VENDOR_PROPERTIES += sys.usb.mtp.batchcancel=1

# USB tethering over NCM, as Pixels: Tethering defaults to RNDIS, which this
# kernel does not build. With the overlay the gadget HAL links ncm.gs6 (created
# by init.qcom.usb.rc) through the "ncm" and "ncm,adb" compositions. The usb0
# interface matches Tethering's default USB pattern.
PRODUCT_PACKAGES += FP6TetheringOverlay
