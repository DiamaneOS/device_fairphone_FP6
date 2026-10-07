# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# GNSS. The GNSS engine runs in the modem, so GNSS works only while the modem
# runs (modem packet; pm-proxy keeps the modem vote). The HAL (android.hardware.gnss
# IGnss v3) and its location libraries are built from CodeLinaro source:
# hardware/qcom/gps and vendor/qcom/opensource/location (DiamaneOS forks),
# libqti_vndfwk_detect_vendor from vendor/qcom/opensource/core-utils. The QMI
# client is the public QMI framework (vendor/qcom/opensource/qmi-framework),
# linked into libloc_api_v02, so the GNSS process loads no Qualcomm closed code.
# The libraries load only the GNSS adapter and the QMI LOC API, and the HAL opens
# no socket for the XTRA and DGNSS daemons: Qualcomm's IZat, XTRA, NTRIP,
# engine-hub, diagnostic, batching and geofence code cannot load.
# gps.conf, izat.conf and sap.conf are Fairphone's stock files (the first two
# with pinned edits by the vendor renderer).
# Assistance comes from the framework: SUPL host (GrapheneOS GnssSettings),
# network time and coarse position requests. PSDS stays off
# (config_gnssPsdsType is empty).

# libgnss and libloc_api_v02 are loaded with dlopen, so they are listed too.
PRODUCT_PACKAGES += \
    android.hardware.gnss-aidl-service-qti \
    android.hardware.gnss-aidl-impl-qti \
    liblocation_api \
    libgnss \
    libloc_core \
    libgps.utils \
    libloc_api_v02 \
    libqti_vndfwk_detect_vendor

# The HAL's state directory; stock creates it from loc-launcher.rc.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/gnss/init.gnss.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.gnss.rc

# GPS feature (identical to the stock vendor file). It also enables the
# SUPL and PSDS settings screens, which require FEATURE_LOCATION_GPS.
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.location.gps.xml
