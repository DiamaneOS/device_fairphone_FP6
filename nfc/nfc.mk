# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# NFC for private bring-up: the stock Samsung S3NRN4V HAL
# (android.hardware.nfc-service.sec, nfc_nci_sec.so), its configuration,
# RF register files and controller firmware come from the selected stock
# vendor files. The NFC AIDL/HIDL interface libraries it links are built from
# source through the generated vendor tree. The NFC stack is the
# com.android.nfcservices apex and SecureElement is the platform app.
#
# Features match what the stock image declares for reader, NDEF and host card
# emulation (stock vendor/etc/permissions/android.hardware.nfc{,.hce,.hcef}.xml,
# byte-for-byte AOSP files). Not declared yet: android.hardware.nfc.uicc and
# android.hardware.se.omapi.uicc (SIM card emulation and OMAPI need the modem,
# the SIM1 secure-element HAL in the radio daemon and a SIM test), and
# android.hardware.nfc.ese / se.omapi.ese (the FP6 has no embedded secure
# element: stock declares none and ships no eSE driver).
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.nfc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/android.hardware.nfc.hcef.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hcef.xml
