# IMS and Wi-Fi calling integration

The FP6 keeps the Qualcomm IMS and radio implementation, with DiamaneOS's
call-audio bridge (`callaudio/`).

## Data-connection broker

- `hardware/diamaneos/ims` provides the data-connection broker (DCM) that the
  stock stack lacks here.
- It serves IMS and EIMS requests through Android networking.
- It authenticates modem traffic by the pinned FP6 QRTR node (modem 0, AP 1;
  `telephony.mk`).
- It has its own vendor UID and a frozen AIDL API, both defined in
  `hardware/diamaneos/ims`.
- It requires enforcing SELinux at runtime.
- Never run a second DCM publisher or infer a modem node from an incoming
  request.

## IWLAN

- The QTI IWLAN frontend binds all three framework interfaces: data service,
  network service and qualified-network selection.
- Its certificate helper shares the same stock-signed application UID and
  process, with no system UID and no privileged Android permission grants.
- The dedicated policy covers only the selected Binder, HIDL and QRTR paths
  (`sepolicy/telephony/fp6_iwlan_app.te`).
- Package-specific hidden-API access supports the stock Java IPC classes
  without platform-signing the apps (`fp6-iwlan-sysconfig.xml`).
- The AOSP IWLAN alternative must not be selected at the same time;
  `telephony.mk` stops the build if it is.

## Wi-Fi reporter

- The source Wi-Fi reporter (`hardware/diamaneos/ims`, selected in
  `telephony.mk`) supplies connected-network observations to DSD.
- It is a separate system_ext daemon with its own application identity.
- It adds no CNE, changes no modem NV settings and does not replace the PDN
  broker.
- The daemon sends only the bounded, reviewed status, settings and
  subscription messages.
- It advertises no unimplemented scan, quality-measurement or keepalive
  capabilities.

## Carrier data

- The vendor generator supplies the exact stock APN table and carrier XML
  assets, consumed by the source CarrierConfig fork.
- The stock carrier-service APK is not installed.
- FP6CarrierConfigOverlay, read after that data, only corrects stock values
  set for every carrier (the CDMA world-phone flag).
- Carrier availability, provisioning and the user's Wi-Fi-calling choice
  remain authoritative.
- The inherited entitlement app uses its source-only HTTPS/polling fork and
  stays idle when the carrier does not configure that flow.
  - It provides neither IMS transport nor AML and cannot replace mandatory
    carrier push support.

## RTT and emergency

- RTT capabilities follow the stock TeleService overlay.
- Emergency routing stays in the Android telephony/Telecom stack and the
  selected radio/IMS implementation.
- Nothing is added: no emergency-number list, local fallback heuristic,
  location recipient or forced provisioning state.
- Existing GNSS/SUPL and modem emergency-location paths keep their selected
  configuration.
- AML is absent.

## Checks

1. Build and check AIDL, VINTF, JNI linking, UID uniqueness, neverallows,
   seapp matching and installed permissions.
2. Run the IMS repository's read-only checker with `--iwlan qti`.
3. On the phone, test ordinary calls, SMS and Wi-Fi calling on both SIMs; keep
   emergency simulations separate from carrier-authorized observations.

Presence checks and host simulations do not establish registration, handover,
call audio or emergency/location delivery.
