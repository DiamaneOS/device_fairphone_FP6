# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Fairphone Camera: Fairphone's own camera app (com.fps.camera, signed by TCL),
# offered in the App Store only on builds that declare the system feature
# de.diamaneos.fairphone_camera (de.diamaneos.fairphone_camera.xml), as this
# one does. The app needs the stock TCL camera algorithm service and its
# libraries, which the tools vendor step selects as
# their own closure (vendor/fairphone/FP6): the service, libtctcameraalgo.so
# and its plugins with their models, and the app's JNI library in system_ext.
# Their GraphicBuffer calls bind uicam (compat/tcl-camera-v34). The portrait
# mode's two-camera live bokeh node runs in the camera provider.
#
# The APK that fits those files is the one the same stock release ships. The
# vendor step therefore declares a second feature in the tree it generates
# (/vendor/etc/permissions/de.diamaneos.fairphone_camera.app.xml), named after
# that APK: de.diamaneos.fairphone_camera.apk_ and the first 16 hex characters
# of its SHA-256. The name is never written here, so it always follows the
# stock files in the image. A catalog version of the app lists both features
# (README.md, Fairphone Camera).
#
# The RAW fusion and HDR libraries (super night, HDR) run on the GPU through
# the Adreno OpenCL runtime, installed as fpcOpenCL.so, the name only the
# service's ANC HDR adapters look for. They convert RAW to YUV on the ISP
# through Qualcomm's post-processing HAL, which the camera provider registers
# for the service only (fpcamera_postproc.xml). The camera provider keeps
# running without OpenCL.
#
# Not selected, because they cannot run with the stock files on this image:
# face super-resolution and the 2x RAW super-resolution (a QNN HTP V73 skeleton
# stock does not ship), the night video and one-camera live bokeh nodes (OpenCL
# in the camera provider) and the raw MFNR (OpenCL in the camera provider). The
# service bypasses an algorithm it does not find, and the app reports a camera
# error for the front portrait and the low-light video setting.
#
# The service runs only when that app asks for it (fpcamera.rc), as its own
# user, in its own domain (sepolicy/fpcamera-vendor). The app runs in its own
# domain without network sockets (sepolicy/fpcamera-system-ext-private), and
# only it can find the service. The camera provider does not use the service.

PRODUCT_PACKAGES += \
    uicam \
    FP6FairphoneCameraOverlay

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/camera/fpcamera/fpcamera.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/fpcamera.rc \
    device/fairphone/FP6/camera/fpcamera/public.libraries.txt:$(TARGET_COPY_OUT_VENDOR)/etc/public.libraries.txt \
    device/fairphone/FP6/camera/fpcamera/de.diamaneos.fairphone_camera.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/de.diamaneos.fairphone_camera.xml

# The ANC HDR library looks for these per camera. Stock ships them empty, and an
# empty file cannot be a selected stock row.
PRODUCT_COPY_FILES += \
    $(foreach c,front_main rear_main rear_uw,device/fairphone/FP6/camera/fpcamera/AncRawAlgos/dgain_mask_$(c).bin:$(TARGET_COPY_OUT_VENDOR)/etc/camera/AncRawAlgos/dgain_mask_$(c).bin)

# RAW fusion picks its 2x RAW super-resolution (ANCSR) from this zoom
# threshold, in tenths. That path cannot run here (its QNN adapter is not
# selected), so the threshold is set above any zoom the app offers: 2x and more
# take the normal path, as without RAW fusion.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.fp.camera.suprsphoto.thresholdx10=1000

# 50 MP: the camera provider accepts a stream of the sensor's full size (8192x6144) only
# with this property, which makes it publish that size for Android's maximum resolution mode.
PRODUCT_VENDOR_PROPERTIES += \
    persist.vendor.camera.capability_ultrahigh=1
