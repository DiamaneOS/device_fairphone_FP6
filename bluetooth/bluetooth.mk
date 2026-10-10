# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project

# Bluetooth. The selected stock vendor files
# supply the Qualcomm HIDL HCI implementation
# (android.hardware.bluetooth@1.1-impl-qti) and its link closure; our own
# service (service.cpp) registers it in place of the stock service, which also
# links the FM, ANT, SAR, config-store and TPI libraries. manifest.xml declares
# IBluetoothHci. The AOSP HCI interfaces are built from source. The service
# installs a seccomp filter before it loads the implementation; its policy
# (bluetooth/seccomp, /vendor/etc/seccomp_policy/bluetooth-hci.policy) comes
# with it as a required module.
# Bluetooth audio is software only (no DSP offload): the AOSP Bluetooth audio
# provider runs in the audio service and the AOSP "bluetooth" audio module
# carries A2DP, hearing-aid and LE audio streams.
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.1-service.fp6 \
    android.hardware.bluetooth.audio-impl \
    audio.bluetooth.default

PRODUCT_COPY_FILES += \
    device/fairphone/FP6/bluetooth/init.fp6.bluetooth.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fp6.bluetooth.rc \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/av/services/audiopolicy/config/bluetooth_with_le_audio_policy_configuration_7_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_with_le_audio_policy_configuration_7_0.xml

# Profiles the AOSP Bluetooth stack enables. It enables none by default.
# Same set as stock (vendor, product and system_ext build.prop) except:
# no SIM access (sap.server) or SIM phonebook (pbap.sim), no AVRCP controller
# (the phone is not an audio sink), no LE audio broadcast yet, and ASHA hearing
# aids added because stock served them through its own stack.
PRODUCT_VENDOR_PROPERTIES += \
    bluetooth.device.class_of_device=90,2,12 \
    bluetooth.profile.a2dp.source.enabled=true \
    bluetooth.profile.asha.central.enabled=true \
    bluetooth.profile.avrcp.target.enabled=true \
    bluetooth.profile.bas.client.enabled=true \
    bluetooth.profile.gatt.enabled=true \
    bluetooth.profile.hfp.ag.enabled=true \
    bluetooth.profile.hid.host.enabled=true \
    bluetooth.profile.map.server.enabled=true \
    bluetooth.profile.opp.enabled=true \
    bluetooth.profile.pan.nap.enabled=true \
    bluetooth.profile.pan.panu.enabled=true \
    bluetooth.profile.pbap.server.enabled=true \
    bluetooth.profile.bap.unicast.client.enabled=true \
    bluetooth.profile.csip.set_coordinator.enabled=true \
    bluetooth.profile.vcp.controller.enabled=true \
    bluetooth.profile.mcp.server.enabled=true \
    bluetooth.profile.ccp.server.enabled=true \
    bluetooth.profile.hap.client.enabled=true

# Stock (system_ext build.prop): up to 10 Bluetooth LE devices connected at
# once instead of 8, on the same controller firmware. A bluetooth_prop, which
# vendor_init may not set, so it is a product property.
PRODUCT_PRODUCT_PROPERTIES += \
    bluetooth.core.le.max_number_of_concurrent_connections=10

# The stock HCI implementation logs the Bluetooth address at info level each
# time Bluetooth starts, when it writes it into the controller's NVM tags
# ("BD Address: ..." in PatchDLManager::ReadTlvInfo and
# NvmTagsManager::DownloadNvmTags), and HCI command dumps at debug level under
# the first tag. Keep both tags at warning; their other info lines (firmware
# versions) go too. The address is the factory one (init.fp6.bluetooth.rc).
PRODUCT_VENDOR_PROPERTIES += \
    log.tag.vendor.qti.bluetooth@1.1-patch_dl_manager=W \
    log.tag.vendor.qti.bluetooth@1.1-nvm_tags_manager=W

# Not set on purpose:
# - ro.bluetooth.a2dp_offload.supported and ro.bluetooth.leaudio_offload.supported
#   (default false): stock offload needs the Qualcomm Bluetooth audio stack.
# - ro.vendor.qti.va_odm.support and persist.vendor.qcom.bluetooth.tpi_supported:
#   only the stock service read them, to register FM, ANT, config-store and TPI
#   services; neither it nor those libraries are installed.
# - persist.vendor.bluetooth.modem_nv_support: would ask the modem for the
#   Bluetooth address over QMI.
# - persist.vendor.service.bdroid.{snooplog,soclog,fwsnoop,dump_uartlogs}:
#   HCI, SoC and UART logging.
# - persist.vendor.service.bdroid.trigger_crash and .ssrlvl: values 1 or 2 make
#   the HCI implementation panic the kernel on a controller failure (default
#   ssrlvl is 3, recovery without panic).
# - persist.vendor.bluetooth.alt_path_for_fw: firmware from /data.
#
# HFP audio gateway stays enabled so headsets connect with their call profile;
# SCO audio is not validated.
