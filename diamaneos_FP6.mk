# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Android framework inheritance comes from the authenticated GrapheneOS tree.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_ramdisk.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_system.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/handheld_system_ext.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony_system_ext.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_product.mk)
$(call inherit-product, device/fairphone/FP6/device.mk)
$(call inherit-product, vendor/diamaneos/product.mk)

PRODUCT_NAME := diamaneos_FP6
PRODUCT_DEVICE := FP6
PRODUCT_MODEL := Fairphone 6
PRODUCT_MANUFACTURER := Fairphone
PRODUCT_BRAND := DiamaneOS
