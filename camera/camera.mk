# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Camera: the stock Qualcomm CamX/CHI provider, its
# Qualcomm components and algorithms, the FP6 sensor module and tuning data,
# the ICP firmware and the CDSP skeletons come from the selected stock vendor
# files (vendor/fairphone/FP6). The AOSP camera interfaces, the HIDL shims and
# the Qualcomm interface libraries the stock blobs link are built from source.
# Not included: the TCL camera algorithm service and its models, third-party
# (ANC/TCL) algorithms, the Fairphone com.fp.node.* feature nodes, QNN/SNPE,
# the always-on (AON) camera and factory (MTF) tuning.

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/camera/init.camera.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.camera.rc

# Seccomp loader for the stock provider (camera/seccomp). The tools renderer
# renames the provider's libhardware.so dependency to libcamxjail.so, which
# installs the filter from /vendor/etc/seccomp_policy/camera-provider.policy
# before the provider's main(). Without it the provider does not start.
PRODUCT_PACKAGES += \
    libcamxjail

# The stock FP6 feature set (vendor/etc/permissions, identical to the AOSP files):
# rear camera with flash and autofocus, front camera, FULL hardware level with
# manual sensor/post-processing, and RAW capture. No concurrent-camera or
# external-camera declaration: stock declares neither.
PRODUCT_COPY_FILES += \
    $(foreach f,flash-autofocus front full raw, \
        frameworks/native/data/etc/android.hardware.camera.$(f).xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.$(f).xml)

# Stock vendor build.prop value: the stock HAL has no HEIC UltraHDR support.
# camera.disable_zsl_mode and ro.camera.enableCamera1MaxZsl (camera1 only) are
# not carried over.
PRODUCT_VENDOR_PROPERTIES += \
    ro.camera.disableHeicUltraHDR=true

# Camera privacy (the Moments switch, the camera access toggle) disconnects
# apps and refuses new opens instead of muting: AOSP's path for cameras
# without mute support. CamX lists a black test pattern but fails when it is
# switched while streaming, and with the kernel camera floor it stays in a
# phase-detect sensor mode the blocked sensor cannot feed. Labelled in
# sepolicy/system-ext-private/property_contexts.
PRODUCT_VENDOR_PROPERTIES += \
    ro.camera.disableCameraMute=true

# Stock (system build.prop): the CamX provider takes the tuning path Fairphone
# shipped and validated. vendor_init may set it (sepolicy/camera/vendor_init.te).
PRODUCT_VENDOR_PROPERTIES += \
    persist.vendor.camera.fprom=1

# Stock (system build.prop): MediaRecorder high-frame-rate recordings keep a
# 60 fps base layer instead of about 33 fps.
PRODUCT_PRODUCT_PROPERTIES += \
    ro.media.recorder-max-base-layer-fps=60

# CamX allocates the preview stream as UBWC NV12 instead of linear NV12
# (CamX outputFormat setting; its compiled default 0 is linear). The display
# hardware can rotate only UBWC buffers inline, so a linear portrait preview
# forced GPU composition for every frame. Previews up to 1088 lines before
# rotation (16:9 1920x1080) are then composed by the display hardware; the 4:3
# photo preview (1600x1200) still needs the GPU. Streams the CPU reads keep
# their own format.
PRODUCT_VENDOR_PROPERTIES += \
    persist.vendor.camera.outputFormat=1

# User builds: no CamX log output. The closed provider logs the three camera
# modules' serial numbers at error level at every start (its EEPROM/OTP
# check), and only silencing the tag stops that. Debuggable builds keep CamX
# logs for diagnosis.
ifeq ($(TARGET_BUILD_VARIANT),user)
PRODUCT_PRODUCT_PROPERTIES += \
    log.tag.CamX=S
endif
