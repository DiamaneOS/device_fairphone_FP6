# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Telephony for bring-up: the stock Qualcomm radio daemon (qcrilNrd),
# its data module and nicmd, the stock IMS app (org.codeaurora.ims) and the
# native modem services come from the
# selected stock files (vendor/fairphone/FP6). The call-audio bridge between the
# radio daemon and the audio HAL is our own (callaudio/, replacing the stock
# QtiTelephonyService). The radio interface libraries, the carrier
# configuration service and telephony features are built from source. Carrier
# data is extracted from the authenticated stock package without its code.
# Native Qualcomm IWLAN is paired with the isolated source reporter and broker.
# Ordinary calling/SMS/data require carrier qualification on the selected image;
# emergency handling is simulated, not end-to-end emergency acceptance.
# AML and eSIM management are deferred. No inactive stock LPA is packaged.

# Dual SIM, dual standby (stock vendor build.prop and system_ext build.prop).
# QCRIL defaults that stock sets in vendor build.prop and that are not in the
# QCRIL database defaults (procedure_bytes) or that the database repeats.
PRODUCT_VENDOR_PROPERTIES += \
    persist.radio.multisim.config=dsds \
    ro.telephony.sim_slots.count=2 \
    telephony.active_modems.max_count=2 \
    persist.vendor.radio.apm_sim_not_pwdn=1 \
    persist.vendor.radio.custom_ecc=1 \
    persist.vendor.radio.enableadvancedscan=true \
    persist.vendor.radio.procedure_bytes=SKIP \
    persist.vendor.radio.sib16_support=1

# Preferred network type for each SIM on first use and after a network reset:
# 26 = NR/LTE/GSM/WCDMA (RILConstants; stock system build.prop). Without
# it the framework falls back to GSM/WCDMA, Settings hides "5G (recommended)"
# and the Allow-2G switch stores a mask with no LTE or NR.
PRODUCT_VENDOR_PROPERTIES += \
    ro.telephony.default_network=26,26

# Without a SIM the lock screen is not forced on (also not over the setup
# wizard): the AOSP phone default from full_base_telephony.mk, which this
# product does not inherit; stock sets it too.
PRODUCT_VENDOR_PROPERTIES += \
    keyguard.no_require_sim=true

# QCRIL copies the APNs Android uses (custom, MVNO, MMS and attach APNs) into
# the modem's data profiles, as stock; its database default (false) keeps the
# modem's built-in carrier profiles. vendor_init may set vendor_dataqdp_prop
# (sepolicy/fp6/platform.te).
PRODUCT_VENDOR_PROPERTIES += \
    persist.vendor.data.profile_update=true

# The slot-0 RIL reports network state 40-150 times a minute while idle. While
# the screen is off, unplugged and in service, poll about once per 5 s and every
# report within 10 s (frameworks/opt/telephony network-state poll coalescing).
# Labelled in sepolicy/telephony-system-ext/property_contexts.
PRODUCT_VENDOR_PROPERTIES += \
    ro.telephony.network_state_poll_window_ms=5000

# Telephony, calling, messaging, data, IMS and eUICC features (AOSP
# definitions; stock declares the same set in vendor/etc/permissions and
# system/etc/permissions/android.hardware.telephony.euicc.xml). No MBMS:
# eMBMS is not installed.
PRODUCT_PACKAGES += \
    android.hardware.telephony.gsm.prebuilt.xml \
    android.hardware.telephony.ims.prebuilt.xml \
    android.hardware.telephony.euicc.prebuilt.xml

# QCRIL directories and database; the stock IMS and LPA apps are only parsed
# when ro.boot.vendor.qspa.modem=enabled (their manifests carry an <overlay
# requiredSystemPropertyName=...> gate), which stock sets from a system_ext
# init script. Only init may set ro.boot.* properties, so the property is set
# from system_ext as on stock.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/telephony/init.fp6.telephony.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.telephony.rc \
    device/fairphone/FP6/telephony/init.fp6.qspa.rc:$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/init/init.fp6.qspa.rc \
    device/fairphone/FP6/telephony/privapp-permissions-fp6-telephony.xml:$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/permissions/privapp-permissions-fp6-telephony.xml

# The QCRIL database we ship has power-up optimisation off (version 16.0). This
# upgrade step brings a version 15.0 copy already in /data/vendor/radio to the
# same state; with the optimisation on, QCRIL holds incoming SMS and USSD until
# a UI-ready call from stock apps we do not ship.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/telephony/qcril-upgrade-0016.0_config.sql:$(TARGET_COPY_OUT_VENDOR)/etc/qcril_database/upgrade/config/0016.0_config.sql

# Call-audio bridge (system_ext privileged app, one normal permission): passes
# each call's vsid/call_state parameters from the radio daemon's IQcRilAudio
# service to the audio HAL. Every call's audio needs it, emergency calls
# included; see callaudio/Android.bp.
PRODUCT_PACKAGES += \
    FP6CallAudio

# Stock modem-backed IWLAN, not the alternative AP-assisted AOSP service.
# The renderer selects IWlanService, CACertService and their exact JNI closure.
# Neither app uses the system UID or our platform key, and neither receives
# privileged Android permissions. Their shared process has a scoped domain.
ifneq ($(filter-out qti,$(DIAMANEOS_IWLAN_IMPLEMENTATION)),)
$(error FP6 selects the QTI IWLAN path; do not also inherit an AOSP IWLAN product)
endif
DIAMANEOS_IWLAN_IMPLEMENTATION := qti
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/telephony/privapp-permissions-fp6-iwlan.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/privapp-permissions-fp6-iwlan.xml \
    device/fairphone/FP6/telephony/fp6-iwlan-sysconfig.xml:$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/sysconfig/fp6-iwlan-sysconfig.xml

# CarrierConfig remains the maintained Android service. The generated
# filegroup contains XML only; CustomerCarrierConfig.apk is never installed.
$(call soong_config_set,diamaneos_carrierconfig,asset_module,fp6_stock_carrier_assets)

# Verified FP6 QRTR topology: AP node 1, modem node 0 (DMS/NAS/WDS/WMS/VOICE
# name-service records). Pin it; never trust the first incoming DCM packet.
# The transport asks the kernel for its local AP node before binding.
DIAMANEOS_IMS_MODEM_NODE := 0
DIAMANEOS_IMS_SLOTS := 2
$(call inherit-product,hardware/diamaneos/ims/ims-product.mk)
$(call inherit-product,hardware/diamaneos/ims/wlan-product.mk)

# APNs come from the authenticated stock XML in vendor/fairphone/FP6. Keep
# IMS/emergency rows, MVNO filters and ordering intact instead of appending
# overlapping entries to the sample database. Users can still add APNs in
# Settings. Existing installations need a new build identity or an owner-run
# APN reset before TelephonyProvider reimports the changed file.

# Device overlay: IMS package and RTT capabilities for TeleService. Stock
# carrier defaults are now extracted as data, including their original filters.
# The CarrierConfig overlay corrects stock values that apply to every carrier
# (see its vendor.xml); it is read after the stock data.
PRODUCT_PACKAGES += \
    FP6TeleServiceOverlay \
    FP6CarrierConfigOverlay
