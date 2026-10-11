# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Video recording, the Iris video core and the hardware video codecs.
#
# Hardware encoders always; hardware decoders as the user chooses in
# Settings > Security & privacy > Exploit protection > Hardware video decoding
# (persist.diamaneos.hw_video_decode, takes effect at the next boot):
# - protected video only, the default: the secure Qualcomm H.264, HEVC and VP9
#   decoders, which read only protected buffers that a DRM session fills
#   (Widevine's hardware-backed library needs them). Clear video stays in the
#   platform software decoders in the sandboxed mediaswcodec;
# - all video: also the three clear decoders (no low-latency variant). That
#   saves battery and plays high-resolution video more smoothly, but lets
#   every video reach closed decoder code, the video driver and the video
#   firmware;
# - never: no hardware decoder, and Widevine protects in software
#   (media/init.fp6.drm.rc).
# Any app that opens a DRM session can still use a secure decoder, so the
# default keeps ordinary video away from the closed decoder code, not every
# app. media/init.fp6.media.rc applies the choice before the codec service and
# the media framework start.
#
# The codec service, its plugins and the Iris firmware (vpu20_2v.mbn) are
# stock (vendor/fairphone/FP6; manifest.xml declares the service's HIDL
# IComponentStore/default). The tools renderer derives three pinned
# configurations from the stock codec list and target specification:
# - _volcano_v1 (never): the target specification names the five encoders and
#   no decoder ("codecs-available"; libqcodec2_v4l2codec registers only the
#   codecs listed), and the codec list has no decoder section, so MediaCodec
#   cannot pick a hardware decoder even if the service listed one;
# - _volcano_v1_secdec (protected video only): the same plus the three secure
#   decoders, in both files. The codec list marks them as requiring secure
#   playback, so MediaCodec does not pick them for clear video;
# - _volcano_v1_hwdec (all video): those plus the three clear decoders.
# The decoder node /dev/video32 is root-only when the choice is never
# (boot/ueventd.rc, media/init.fp6.media.rc), and only the codec service may
# open the two codec nodes (sepolicy/media). The secure decoders also use the
# memory-buffer driver's bitstream node (boot/ueventd.rc, sepolicy/media).
# The Qualcomm library enables every codec if it cannot read its target
# specification, so the codec service loads libc2hwjail_avservices, which
# checks this boot's state and offers only the allowed codecs, or none (see
# media/seccomp/c2hwjail.cpp).
# Not included: the Codec2 audio service (c2audio), the OMX core, Wi-Fi
# display, VPP and video power optimisation.

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/media/init.fp6.media.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.media.rc

# DRM HALs (android.hardware.drm). ClearKey is built from the platform source;
# media/init.fp6.drm.rc runs it without the group that owns the TEE node.
# Widevine is Fairphone's stock package, which the generated vendor tree
# installs (vendor/fairphone/FP6) together with the stock hardware-backed
# library (liboemcrypto.so) in /vendor/lib64; the same rc file defines its
# service twice, with that library or with the service's built-in software
# protection when the user chose no hardware video decoding. Each runs in
# its own domain: sepolicy/media/hal_drm_widevine.te and
# hal_drm_widevine_sw.te.
PRODUCT_PACKAGES += \
    android.hardware.drm-service.clearkey

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/media/init.fp6.drm.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.drm.rc

# The Widevine package may load a library from /vendor/lib64 only if the
# vendor linker configuration offers it by name.
PRODUCT_VENDOR_LINKER_CONFIG_FRAGMENTS += \
    device/fairphone/FP6/media/linker.config.widevine.json

# The hardware-backed library's two stock settings: tell the Widevine trusted
# application that keys are provisioned with the device's certificate chain
# (no factory keybox), and decrypt clear-output content through the crypto
# engine.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.wv.oemcrypto.debug.enable_prov40=true \
    vendor.wv.oemcrypto.debug.enable_hlos_data_path=true

# Android 14 ABI compat libraries for the stock Codec2 framework libraries
# (see compat/codec2-v34). The tools renderer renames one dependency of
# libcodec2_vndk (libui.so, plus its GraphicBuffer imports) and of
# libcodec2_hidl@1.0-1.2 (libstagefright_bufferqueue_helper.so) to them.
PRODUCT_PACKAGES += \
    uiv34 \
    libstagefright_bqhelper_v34compat

# Seccomp filter and codec gate for the codec service (media/seccomp). The
# tools renderer renames the service's libavservices_minijail.so dependency
# to it; without it the service does not start.
PRODUCT_PACKAGES += \
    libc2hwjail_avservices

# SoC video variant. Stock init.qti.media.sh derives it at boot from
# /sys/devices/soc0/soc_id and the video core's fused SKU
# (/sys/devices/platform/soc/aa00000.qcom,vidc/sku_version): for the SM7635
# SoC ids (636, 640, 641, 712) sku_version 1 selects _volcano_v1 (the
# IRIS_MULTIPIPE_DISABLE fuse, 4K30 encode), anything else _volcano_v0. The
# phone reads soc_id 636 and sku_version 1, so the value
# is fixed here instead of running a shell script at boot; only the
# _volcano_v1 files are selected.
FP6_MEDIA_VARIANT := _volcano_v1
# vendor.media.target_variant: libqcodec2_platform reads
# /vendor/etc/media$(FP6_MEDIA_VARIANT)/video_system_specs.json.
# ro.media.xml_variant.*: the framework reads
# /vendor/etc/media_codecs$(FP6_MEDIA_VARIANT).xml and
# media_codecs_performance$(FP6_MEDIA_VARIANT).xml; without them it lists no
# hardware codec at all. ro.media.xml_variant.codecs is not set here:
# media/init.fp6.media.rc sets it at early-boot from the user's choice and,
# for the two choices with hardware decoders, switches
# vendor.media.target_variant to $(FP6_MEDIA_VARIANT)_secdec or _hwdec. The
# performance list is the same in every state.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.media.target_variant=$(FP6_MEDIA_VARIANT) \
    ro.media.xml_variant.codecs_performance=$(FP6_MEDIA_VARIANT)

# Stock vendor build.prop values. Both are debug_prop (platform
# property_contexts "debug." prefix): every domain may read them and
# vendor_init may set them.
# c2inputsurface=-1: MediaCodec.createPersistentInputSurface() (used by the
# camera app's CameraX recorder) builds the input surface in the calling
# process instead of asking the OMX service, which this product (like stock)
# does not have. Without it recording fails with "Failed to connect to OMX to
# create persistent input surface" and CameraX error 7. It also keeps the
# framework from ever calling the store's createInputSurface (the one user of
# the bqhelper compat library).
# use_dmabufheaps=1: Codec2 allocates linear buffers from DMA-BUF heaps (GKI
# has no ION).
# debug.stagefright.ccodec and omx_default_rank (OMX only) are not carried over.
PRODUCT_VENDOR_PROPERTIES += \
    debug.stagefright.c2inputsurface=-1 \
    debug.c2.use_dmabufheaps=1
