# IMS and Wi-Fi calling integration

The FP6 keeps the Qualcomm IMS and radio implementation, with DiamaneOS's
call-audio bridge (`callaudio/`).

## Data-connection broker

- `hardware/diamaneos/ims` provides the data-connection broker (DCM) that the
  stock stack lacks here.
- It serves IMS and EIMS requests through Android networking.
- It authenticates modem traffic by the pinned FP6 QRTR node (modem 0, AP 1,
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
  without platform-signing the apps.
- The AOSP IWLAN alternative must not be selected at the same time.

The opt-in source Wi-Fi reporter supplies connected-network observations to DSD
through a separate system_ext daemon and application identity. It does not add
CNE, change modem NV settings or replace the PDN broker. The daemon only sends
the bounded, reviewed status/settings/subscription messages; it does not advertise
unimplemented scan, quality-measurement or keepalive capabilities. Cold-start
reconciliation and complete carrier behavior remain qualification
requirements; this configuration is not a release acceptance claim.

The vendor generator supplies the exact stock APN table and carrier XML assets.
The source CarrierConfig fork consumes that data; the stock carrier-service APK
is not installed. This replaces the earlier partial FP6CarrierConfigOverlay.
Carrier availability, provisioning and the user's Wi-Fi-calling choice remain
authoritative. The inherited entitlement app uses its source-only HTTPS/polling
fork; it stays idle when the carrier does not configure that flow. It provides
neither IMS transport nor AML, and cannot replace mandatory carrier push support.

RTT capabilities follow the stock TeleService overlay. Emergency routing remains
in the maintained Android telephony/Telecom stack and the selected radio/IMS
implementation; no new emergency-number list, local fallback heuristic,
location recipient or forced provisioning state is added. Existing GNSS/SUPL
and modem emergency-location paths keep their selected configuration; AML is
absent.

## Checks

1. Build and check AIDL, VINTF, JNI linking, UID uniqueness, neverallows,
   seapp matching and installed permissions.
2. Run the IMS repository's read-only checker with `--iwlan qti`.
3. On the phone, test ordinary calls, SMS and Wi-Fi calling on both SIMs. Keep
   emergency simulations separate from carrier-authorized observations.

Presence checks and host simulations do not establish registration, handover,
call audio or emergency/location delivery.
