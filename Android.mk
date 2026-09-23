# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_DEVICE),FP6)

# The vendor image is read-only, so the fstab.qcom mount points must exist at
# build time. Make modules are used because the Soong copy-file conversion maps
# vendor/firmware_mnt onto vendor/firmware and rejects it.
define fp6-vendor-mount-point
include $(CLEAR_VARS)
LOCAL_MODULE := fp6_vendor_mount_point_$(1)
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/LICENSE
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/$(1)
LOCAL_MODULE_STEM := .mountpoint
LOCAL_SRC_FILES := boot/mountpoint
include $(BUILD_PREBUILT)
endef

$(foreach d,firmware_mnt dsp bt_firmware,$(eval $(call fp6-vendor-mount-point,$(d))))

endif
