# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# GNSS for private bring-up. The GNSS engine runs in the modem, so GNSS works
# only while the modem runs (modem packet; pm-proxy keeps the modem vote). The
# stock Qualcomm GNSS HAL (android.hardware.gnss IGnss v3) and the location
# libraries it links or loads come from the selected stock vendor files
# (connectivity-peripherals, userspace_hal_families:gnss); the frozen AIDL/HIDL
# interface libraries it links are built from source. izat.conf, gps.conf and
# the service rc are pinned derivations made by the vendor renderer.
# Not included: loc_launcher and every daemon it starts (XTRA download, LOWI
# Wi-Fi scanning, Wi-Fi/cell crowdsourcing, sensor, correction and engine
# services), the Qualcomm IZat/LBS layer, the vendor.qti.gnss service and the
# batching and geofence adapters.
# Assistance comes from the framework: SUPL host (GrapheneOS GnssSettings),
# network time and coarse position requests. PSDS stays off
# (config_gnssPsdsType is empty).

# Directories the HAL uses; stock creates them from loc-launcher.rc.
PRODUCT_COPY_FILES += \
    device/fairphone/FP6/gnss/init.gnss.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.gnss.rc

# GPS feature (identical to the stock vendor file). It also enables the
# SUPL and PSDS settings screens, which require FEATURE_LOCATION_GPS.
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.location.gps.xml
