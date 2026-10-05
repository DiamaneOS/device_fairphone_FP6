# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Video recording, the Iris video core and the hardware video encoders.
# Hardware encoders only (owner decision 2026-09-27): every decoder stays the
# platform software decoder in the sandboxed mediaswcodec
# (com.android.media.swcodec). Decoders parse untrusted media; hardware
# decoders would move that parsing into closed code, the video driver and the
# video firmware.
#
# The stock Qualcomm Codec2 service (vendor.qti.media.c2@1.0-service, HIDL
# IComponentStore/default, declared in manifest.xml), its libqcodec2 plugins,
# the Android 14 Codec2 framework libraries they were built against, the
# SM7635 codec list and target specification and the Iris firmware
# (vpu20_2v.mbn, which msm_video.ko requests at probe) come from the selected
# stock vendor files (vendor/fairphone/FP6). The frozen Codec2 HIDL and
# bufferpool2 AIDL interfaces are built from source. No hardware decoder is
# reachable:
# - the service registers only the codecs its target specification names
#   ("codecs-available"); the tools renderer installs a pinned copy of the
#   stock specification that names the five encoders and no decoder, and
#   libqcodec2_v4l2codec skips every codec not listed;
# - the codec list (media_codecs_volcano_v1.xml) is a pinned copy without the
#   decoder section, so MediaCodec cannot pick a hardware decoder even if the
#   service listed one;
# - the decoder node /dev/video32 is root-only (boot/ueventd.rc), so no
#   process but root can open it.
# If the service cannot read its target specification (property unreadable,
# file missing or not parsable) the Qualcomm library enables every codec and
# logs "List of Supported Codecs is Empty"; the other two layers still keep
# decoders out, and the phone checks look for that line.
# Not included: the Codec2 audio service (c2audio), the OMX core, Wi-Fi
# display, VPP, video power optimisation and secure (DRM) video.

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/media/init.fp6.media.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.media.rc

# Android 14 ABI compat libraries for the stock Codec2 framework libraries
# (see compat/codec2-v34). The tools renderer renames one dependency of
# libcodec2_vndk (libui.so, plus its GraphicBuffer imports) and of
# libcodec2_hidl@1.0-1.2 (libstagefright_bufferqueue_helper.so) to them.
PRODUCT_PACKAGES += \
    uiv34 \
    libstagefright_bqhelper_v34compat

# SoC video variant. Stock init.qti.media.sh derives it at boot from
# /sys/devices/soc0/soc_id and the video core's fused SKU
# (/sys/devices/platform/soc/aa00000.qcom,vidc/sku_version): for the SM7635
# SoC ids (636, 640, 641, 712) sku_version 1 selects _volcano_v1 (the
# IRIS_MULTIPIPE_DISABLE fuse, 4K30 encode), anything else _volcano_v0. The
# phone reads soc_id 636 and sku_version 1 (checked 2026-09-27), so the value
# is fixed here instead of running a shell script at boot; only the
# _volcano_v1 files are selected.
FP6_MEDIA_VARIANT := _volcano_v1
# vendor.media.target_variant: libqcodec2_platform reads
# /vendor/etc/media$(FP6_MEDIA_VARIANT)/video_system_specs.json.
# ro.media.xml_variant.*: the framework reads
# /vendor/etc/media_codecs$(FP6_MEDIA_VARIANT).xml and
# media_codecs_performance$(FP6_MEDIA_VARIANT).xml (stock init.qti.media.rc
# copies the first into the other two at post-fs-data). Without them the
# hardware encoders are not listed at all.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.media.target_variant=$(FP6_MEDIA_VARIANT) \
    ro.media.xml_variant.codecs=$(FP6_MEDIA_VARIANT) \
    ro.media.xml_variant.codecs_performance=$(FP6_MEDIA_VARIANT)

# Stock vendor build.prop values (lines 341 and 343). Both are debug_prop
# (platform property_contexts "debug." prefix): every domain may read it and
# vendor_init may set it.
# c2inputsurface=-1: MediaCodec.createPersistentInputSurface() (used by the
# camera app's CameraX recorder) builds the input surface in the calling
# process instead of asking the OMX service, which this product (like stock)
# does not have. Without it recording fails with "Failed to connect to OMX to
# create persistent input surface" and CameraX error 7 (camera test). It also
# keeps the framework from ever calling the store's createInputSurface (the
# one user of the bqhelper compat library).
# use_dmabufheaps=1: Codec2 allocates linear buffers from DMA-BUF heaps (GKI
# has no ION).
# debug.stagefright.ccodec and omx_default_rank (OMX only) are not carried over.
PRODUCT_VENDOR_PROPERTIES += \
    debug.stagefright.c2inputsurface=-1 \
    debug.c2.use_dmabufheaps=1
