# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

DEVICE_PATH := device/fairphone/FP6

TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a-branchprot
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_VARIANT := generic
TARGET_BOARD_PLATFORM := volcano
TARGET_BOOTLOADER_BOARD_NAME := fps
# Keep the authenticated device bootloader. This product builds Android images.
TARGET_NO_BOOTLOADER := true
DEVICE_MANIFEST_FILE := $(DEVICE_PATH)/manifest.xml
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE := $(DEVICE_PATH)/framework_compatibility_matrix.xml

TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_ODM := odm
TARGET_COPY_OUT_PRODUCT := product
TARGET_COPY_OUT_SYSTEM_EXT := system_ext
TARGET_COPY_OUT_VENDOR_DLKM := vendor_dlkm
TARGET_COPY_OUT_SYSTEM_DLKM := system_dlkm

# These are device partition capacities, not workstation paths or output sizes.
BOARD_BOOTIMAGE_PARTITION_SIZE := 100663296
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 100663296
BOARD_INIT_BOOT_IMAGE_PARTITION_SIZE := 8388608
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 104857600
BOARD_DTBOIMG_PARTITION_SIZE := 31457280
BOARD_PVMFWIMAGE_PARTITION_SIZE := 1048576
BOARD_SUPER_PARTITION_SIZE := 9663676416
BOARD_SUPER_PARTITION_GROUPS := diamaneos_dynamic_partitions
BOARD_DIAMANEOS_DYNAMIC_PARTITIONS_SIZE := 9659482112
BOARD_DIAMANEOS_DYNAMIC_PARTITIONS_PARTITION_LIST := system system_ext product vendor odm vendor_dlkm system_dlkm

TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEM_EXTIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_PRODUCTIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_ODMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEM_DLKMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_USES_VENDOR_DLKMIMAGE := true
BOARD_USES_SYSTEM_DLKMIMAGE := true
BOARD_USES_METADATA_PARTITION := true

BOARD_BOOT_HEADER_VERSION := 4
BOARD_INIT_BOOT_HEADER_VERSION := 4
BOARD_USES_GENERIC_KERNEL_IMAGE := true
BOARD_KERNEL_PAGESIZE := 4096
BOARD_RAMDISK_USE_LZ4 := true
# Stock boot v4 carries the kernel. init_boot and recovery carry ramdisks only.
BOARD_EXCLUDE_KERNEL_FROM_RECOVERY_IMAGE := true
# Recovery is a complete A/B partition image, not an imgdiff patch from boot.
BOARD_USES_FULL_RECOVERY_IMAGE := true
BOARD_KERNEL_BASE := 0x00000000
BOARD_MKBOOTIMG_ARGS += --header_version 4 --kernel_offset 0x00008000
BOARD_MKBOOTIMG_ARGS += --ramdisk_offset 0x01000000 --tags_offset 0x00000100
BOARD_MKBOOTIMG_ARGS += --dtb_offset 0x01f00000
BOARD_MKBOOTIMG_INIT_ARGS += --header_version 4
# GKI v4 loaders obtain versions from AVB. Stock uses zero in these headers.
# These trailing arguments override the platform's legacy header defaults.
BOARD_MKBOOTIMG_ARGS += --os_version 0 --os_patch_level 0
BOARD_MKBOOTIMG_INIT_ARGS += --os_version 0 --os_patch_level 0
# Keep recovery and Android on the same mount and encryption definitions.
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/boot/fstab.qcom
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
TARGET_RECOVERY_UI_SCREEN_WIDTH := 1080
TARGET_RECOVERY_UI_MARGIN_HEIGHT := 75
TARGET_RECOVERY_UI_BRIGHTNESS_FILE := /sys/class/backlight/panel0-backlight/brightness
TARGET_RECOVERY_UI_MAX_BRIGHTNESS_FILE := /sys/class/backlight/panel0-backlight/max_brightness

# Device-specific fstab, HAL manifests and policy are reviewed with the selected
# vendor closure. Kernel offsets, DT packing and module load lists belong to
# the generated kernel integration. Neither include is optional.
include vendor/fairphone/FP6/BoardConfigVendor.mk
include device/fairphone/FP6-kernel/BoardConfigKernel.mk

# Reviewed source policy belongs to the device tree, not generated stock files.
BOARD_VENDOR_SEPOLICY_DIRS += $(addprefix $(DEVICE_PATH)/sepolicy/,vendor-common vendor-attributes qva-common vendor-volcano qva-volcano)
SYSTEM_EXT_PUBLIC_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/system-ext-public
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/system-ext-private
PRODUCT_PUBLIC_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/product-public
PRODUCT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/product-private

# Vendor user of the boot control HAL. boot/ueventd.rc gives it its nodes.
TARGET_FS_CONFIG_GEN += hardware/qcom/bootctrl/aidl/config.fs
# AVB is enabled. Release signing replaces the test identities.
BOARD_AVB_ENABLE := true
BOARD_AVB_KEY_PATH := external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_ALGORITHM := SHA256_RSA4096
# The top-level vbmeta covers vendor, odm, the dlkm images, dtbo and
# vendor_boot. It uses location 0, as on Pixels. Stock stores 0 there, so a
# return to stock needs `fastboot oem reset-rollback` before relocking.
BOARD_AVB_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)

# Chains as on stock, at the top rollback index locations (the bootloader keeps
# 32). Stock uses 1 to 4 for its chains.
# The kernel follows the selected vendor source baseline. The generic ramdisk
# follows the platform. Header zeros must not erase their AVB version metadata.
BOOT_SECURITY_PATCH := $(VENDOR_SECURITY_PATCH)
INIT_BOOT_SECURITY_PATCH := $(PLATFORM_SECURITY_PATCH)
BOARD_AVB_RECOVERY_KEY_PATH := $(BOARD_AVB_KEY_PATH)
BOARD_AVB_RECOVERY_ALGORITHM := $(BOARD_AVB_ALGORITHM)
BOARD_AVB_RECOVERY_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_RECOVERY_ROLLBACK_INDEX_LOCATION := 28
BOARD_AVB_BOOT_KEY_PATH := $(BOARD_AVB_KEY_PATH)
BOARD_AVB_BOOT_ALGORITHM := $(BOARD_AVB_ALGORITHM)
BOARD_AVB_BOOT_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_BOOT_ROLLBACK_INDEX_LOCATION := 30
BOARD_AVB_INIT_BOOT_KEY_PATH := $(BOARD_AVB_KEY_PATH)
BOARD_AVB_INIT_BOOT_ALGORITHM := $(BOARD_AVB_ALGORITHM)
BOARD_AVB_INIT_BOOT_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_INIT_BOOT_ROLLBACK_INDEX_LOCATION := 31

# Match the fstab's system-side AVB chain. Generated vendor inputs must not
# redefine its location.
# The FP6 bootloader requests pvmfw whenever its partition exists and rejects a
# verified slot that does not describe it, so the chain must include pvmfw.
BOARD_AVB_VBMETA_SYSTEM := system system_ext product pvmfw
BOARD_AVB_VBMETA_SYSTEM_KEY_PATH := $(BOARD_AVB_KEY_PATH)
BOARD_AVB_VBMETA_SYSTEM_ALGORITHM := $(BOARD_AVB_ALGORITHM)
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX_LOCATION := 29

# dm-verity hash trees use SHA-256, as on stock. avbtool still defaults to
# SHA-1 for backwards compatibility.
BOARD_AVB_SYSTEM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_SYSTEM_EXT_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_PRODUCT_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_VENDOR_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_ODM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_VENDOR_DLKM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_SYSTEM_DLKM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256

# The FP6 bootloader appends androidboot.fstab_suffix itself, and the kernel
# rejects the whole bootconfig when a key is defined twice. Provide only the
# hardware name. fstab lookup falls back from the bootloader's suffix to
# fstab.qcom.
BOARD_BOOTCONFIG += androidboot.hardware=qcom

# The published FP6 recovery extension uses UFS BSG, not the legacy SG ABI.
SOONG_CONFIG_NAMESPACES += ufsbsg
SOONG_CONFIG_ufsbsg += ufsframework
SOONG_CONFIG_ufsbsg_ufsframework := bsg

# Protect the source-built Qualcomm boot-control service and GPT/UFS helpers.
CFI_INCLUDE_PATHS += hardware/qcom/bootctrl vendor/qcom/opensource/recovery-ext
