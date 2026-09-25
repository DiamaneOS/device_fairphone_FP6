# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Telephony for private bring-up: the stock Qualcomm radio daemon (qcrilNrd),
# its data module and nicmd, the stock IMS app (org.codeaurora.ims), the QCRIL
# audio messenger (QtiTelephonyService) and the stock eSIM LPA
# (com.qualcomm.qti.lpa, disabled by default) come from the selected stock
# files (vendor/fairphone/FP6). The radio interface libraries, the carrier
# configuration app, the APN list and the telephony features are AOSP and built
# from source. Not included yet: VoWiFi (IWLAN), video calls, RCS, an eSIM
# download UI, the SIM secure element (OMAPI UICC).

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
# 26 = NR/LTE/TD-SCDMA/CDMA/EvDo/GSM/WCDMA (stock system build.prop). Without
# it the framework falls back to GSM/WCDMA, Settings hides "5G (recommended)"
# and the Allow-2G switch stores a mask with no LTE or NR.
PRODUCT_VENDOR_PROPERTIES += \
    ro.telephony.default_network=26,26

# Telephony, calling, messaging, data, IMS and eUICC features (AOSP
# definitions; stock declares the same set in vendor/etc/permissions and
# system/etc/permissions/android.hardware.telephony.euicc.xml). No MBMS:
# eMBMS is not installed.
PRODUCT_PACKAGES += \
    android.hardware.telephony.gsm.prebuilt.xml \
    android.hardware.telephony.ims.prebuilt.xml \
    android.hardware.telephony.euicc.prebuilt.xml

# QCRIL directories and database; the stock IMS, audio-messenger and LPA apps
# are only parsed when ro.boot.vendor.qspa.modem=enabled (their manifests carry
# an <overlay requiredSystemPropertyName=...> gate), which stock sets from a
# system_ext init script. Only init may set ro.boot.* properties, so the
# property is set from system_ext as on stock.
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

# eSIM LPA (product priv-app): its privileged-permission allowlist and the
# default-disabled state of its services (product apps take their allowlist
# from the product partition).
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/telephony/privapp-permissions-fp6-lpa.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/permissions/privapp-permissions-fp6-lpa.xml \
    device/fairphone/FP6/telephony/fp6-lpa-default-disabled.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/sysconfig/fp6-lpa-default-disabled.xml

# APNs: the AOSP sample database (source equivalent of the stock
# product/etc/apns-conf.xml); users can still add APNs in Settings. The
# telephony provider imports this file only when ro.build.id changes, so a
# phone that booted a build without it keeps an empty APN table until APNs are
# reset to default in Settings.
PRODUCT_COPY_FILES += \
    device/sample/etc/apns-full-conf.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/apns-conf.xml

# Device overlays: IMS package for TeleService and the stock global VoLTE and
# emergency-domain carrier defaults for the AOSP CarrierConfig app (stock FP6
# ships them in its own carrier-config app, com.fp.customercarrierconfig
# res/xml/vendor.xml).
PRODUCT_PACKAGES += \
    FP6TeleServiceOverlay \
    FP6CarrierConfigOverlay
