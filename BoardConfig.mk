# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

DEVICE_PATH := device/fairphone/FP6

TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a-branchprot
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_VARIANT := generic
TARGET_BOARD_PLATFORM := volcano
TARGET_BOOTLOADER_BOARD_NAME := fps
# Keep the authenticated device bootloader; this product builds Android images.
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
# Stock boot v4 carries the kernel; init_boot and recovery carry ramdisks only.
BOARD_EXCLUDE_KERNEL_FROM_RECOVERY_IMAGE := true
# Recovery is a complete A/B partition image, not an imgdiff patch from boot.
BOARD_USES_FULL_RECOVERY_IMAGE := true
BOARD_KERNEL_BASE := 0x00000000
BOARD_MKBOOTIMG_ARGS += --header_version 4 --kernel_offset 0x00008000
BOARD_MKBOOTIMG_ARGS += --ramdisk_offset 0x01000000 --tags_offset 0x00000100
BOARD_MKBOOTIMG_ARGS += --dtb_offset 0x01f00000
BOARD_MKBOOTIMG_INIT_ARGS += --header_version 4
# GKI v4 loaders obtain versions from AVB; stock uses zero in these headers.
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
# Device-owned grants for the FP6 services, reviewed per denial.
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/fp6
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/audio
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/modem
SYSTEM_EXT_PUBLIC_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/system-ext-public
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/system-ext-private
PRODUCT_PUBLIC_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/product-public
PRODUCT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/product-private

# Development identity only. AVB remains enabled; a later signing workflow
# replaces test identities rather than relabelling these artifacts.
BOARD_AVB_ENABLE := true
BOARD_AVB_KEY_PATH := external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_ALGORITHM := SHA256_RSA4096

# Preserve the published FP6 boot/recovery chains and rollback locations.
# The kernel follows the selected vendor source baseline; the generic ramdisk
# follows the platform. Header zeros must not erase their AVB version metadata.
BOOT_SECURITY_PATCH := $(VENDOR_SECURITY_PATCH)
INIT_BOOT_SECURITY_PATCH := $(PLATFORM_SECURITY_PATCH)
BOARD_AVB_RECOVERY_KEY_PATH := $(BOARD_AVB_KEY_PATH)
BOARD_AVB_RECOVERY_ALGORITHM := $(BOARD_AVB_ALGORITHM)
BOARD_AVB_RECOVERY_ROLLBACK_INDEX := 1
BOARD_AVB_RECOVERY_ROLLBACK_INDEX_LOCATION := 1
BOARD_AVB_BOOT_KEY_PATH := $(BOARD_AVB_KEY_PATH)
BOARD_AVB_BOOT_ALGORITHM := $(BOARD_AVB_ALGORITHM)
BOARD_AVB_BOOT_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_BOOT_ROLLBACK_INDEX_LOCATION := 3
BOARD_AVB_INIT_BOOT_KEY_PATH := $(BOARD_AVB_KEY_PATH)
BOARD_AVB_INIT_BOOT_ALGORITHM := $(BOARD_AVB_ALGORITHM)
BOARD_AVB_INIT_BOOT_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_INIT_BOOT_ROLLBACK_INDEX_LOCATION := 4

# Match the fstab's system-side AVB chain. The location is the published FP6
# system-chain slot; generated vendor inputs must not redefine this identity.
# The FP6 bootloader requests pvmfw whenever its partition exists and rejects a
# verified slot that does not describe it, so the chain must include pvmfw.
BOARD_AVB_VBMETA_SYSTEM := system system_ext product pvmfw
BOARD_AVB_VBMETA_SYSTEM_KEY_PATH := $(BOARD_AVB_KEY_PATH)
BOARD_AVB_VBMETA_SYSTEM_ALGORITHM := $(BOARD_AVB_ALGORITHM)
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX_LOCATION := 2

# The FP6 bootloader appends androidboot.fstab_suffix itself, and the kernel
# rejects the whole bootconfig when a key is defined twice. Provide only the
# hardware name; fstab lookup falls back from the bootloader's suffix to
# fstab.qcom.
BOARD_BOOTCONFIG += androidboot.hardware=qcom

# Debuggable bring-up builds only: boot SELinux permissive so denials are logged
# without blocking boot. Android ignores this key on user builds.
ifneq ($(TARGET_BUILD_VARIANT),user)
BOARD_BOOTCONFIG += androidboot.selinux=permissive
endif

# The published FP6 recovery extension uses UFS BSG, not the legacy SG ABI.
SOONG_CONFIG_NAMESPACES += ufsbsg
SOONG_CONFIG_ufsbsg += ufsframework
SOONG_CONFIG_ufsbsg_ufsframework := bsg

# Protect the source-built Qualcomm boot-control service and GPT/UFS helpers.
CFI_INCLUDE_PATHS += hardware/qcom/bootctrl vendor/qcom/opensource/recovery-ext

# Wi-Fi (QCA6750, qcacld-3.0). The HAL writes ON/OFF to the driver's /dev/wlan
# node and waits for the driver to finish probing before bringing up wlan0.
BOARD_WLAN_DEVICE := qcwcn
WIFI_DRIVER_STATE_CTRL_PARAM := "/dev/wlan"
WIFI_DRIVER_STATE_ON := "ON"
WIFI_DRIVER_STATE_OFF := "OFF"
# The QCA6750 driver reports its interface combinations and the Wi-Fi service
# reloads them into a single combination mode after first configuration; start
# in that same mode (the legacy STA/AP default modes cannot be re-selected).
WIFI_HAL_INTERFACE_COMBINATIONS := {{{STA}, 1}, {{AP}, 1}, {{P2P, NAN}, 1}}
