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

# The TEE refuses the remote key provisioning request on a phone with a custom
# AVB key, so the keystore never gets attestation keys: no Auditor and no
# de.diamaneos.hardware_attestation feature.
DIAMANEOS_HARDWARE_ATTESTATION := false
$(call inherit-product, vendor/diamaneos/product.mk)

# Fairphone's stock product identity, as GrapheneOS keeps Google's for Pixels:
# Google blocks a DiamaneOS-branded identity as an uncertified device.
# DiamaneOS stays the name people see (setup, About, system label, boot); the
# build ID, number, variant and keys in the fingerprint are DiamaneOS's own.
PRODUCT_NAME := FP6
PRODUCT_DEVICE := FP6
PRODUCT_MODEL := The Fairphone (Gen. 6)
PRODUCT_MANUFACTURER := Fairphone
PRODUCT_BRAND := Fairphone
