# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Video recording and the Iris video core, stage A (reviews/OP-HW-BRINGUP
# media, spec.md section 0). Every codec stays the platform software codec
# (com.android.media.swcodec). The selected stock vendor files add only the
# Iris firmware (vendor/firmware/vpu20_2v.mbn), which msm_video.ko requests at
# probe. The stock Qualcomm Codec2 service comes in stage B, together with
# its ABI shims (compat/codec2-v34).

# Stock vendor build.prop values (lines 341 and 343). Both are debug_prop
# (platform property_contexts "debug." prefix): every domain may read it and
# vendor_init may set it.
# c2inputsurface=-1: MediaCodec.createPersistentInputSurface() (used by the
# camera app's CameraX recorder) builds the input surface in the calling
# process instead of asking the OMX service, which this product (like stock)
# does not have. Without it recording fails with "Failed to connect to OMX to
# create persistent input surface" and CameraX error 7 (r9p camera test).
# use_dmabufheaps=1: Codec2 allocates linear buffers from DMA-BUF heaps (GKI
# has no ION).
# debug.stagefright.ccodec and omx_default_rank (OMX only) are not carried over.
PRODUCT_VENDOR_PROPERTIES += \
    debug.stagefright.c2inputsurface=-1 \
    debug.c2.use_dmabufheaps=1
