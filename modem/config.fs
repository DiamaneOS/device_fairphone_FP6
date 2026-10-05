# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project
#
# Vendor users and groups of the remote-processor services (init.modem.rc,
# boot/init.qcom.rc), in the OEM range. Stock FP6 /vendor/etc/passwd uses
# 2901-2917; hardware/diamaneos/ims uses 2990 (IMS DCM daemon) and 2991 (Wi-Fi
# reporter).
# BoardConfig.mk adds this file to TARGET_FS_CONFIG_GEN.

# tqftpserv: the remote processors' TFTP file server.
[AID_VENDOR_TQFTPSERV]
value: 2993

# pd-mapper: the protection-domain locator.
[AID_VENDOR_PD_MAPPER]
value: 2992
