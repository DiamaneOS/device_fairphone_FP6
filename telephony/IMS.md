# IMS and Wi-Fi calling integration

This candidate keeps the Qualcomm IMS/radio implementation and call-audio
bridge.

`hardware/diamaneos/ims` replaces the missing data-connection broker. It serves
IMS/EIMS requests through Android networking and authenticates modem traffic by
the verified FP6 QRTR node (modem 0, AP 1). Vendor UID 2990, AIDL API frozen at
V1, enforcing SELinux required at runtime. Never run a second DCM publisher or
infer a modem node from an incoming request.

The QTI IWLAN frontend binds all three framework interfaces: data service,
network service and qualified-network selection.

- Its certificate helper shares the same stock-signed application UID/process,
  with no system UID or privileged Android permission grants.
- The dedicated policy covers only the selected Binder, HIDL and QRTR paths.
- Package-specific hidden-API access supports the stock Java IPC classes
  without platform-signing the apps.
- The AOSP IWLAN alternative must not be selected at the same time.

The opt-in source Wi-Fi reporter supplies connected-network observations to DSD
through a separate system_ext daemon and application identity. It adds no CNE,
changes no modem NV settings and does not replace the PDN broker. The daemon
sends only the bounded, reviewed status/settings/subscription messages and
advertises no unimplemented scan, quality-measurement or keepalive
capabilities. Cold-start reconciliation and complete carrier behaviour remain
qualification requirements; this configuration is no release acceptance claim.

The vendor generator supplies the exact stock APN table and carrier XML assets,
consumed by the source CarrierConfig fork; the stock carrier-service APK is not
installed.

- FP6CarrierConfigOverlay, read after that data, only corrects stock values set
  for every carrier (the CDMA world-phone flag).
- Carrier availability, provisioning and the user's Wi-Fi-calling choice remain
  authoritative.
- The inherited entitlement app uses its source-only HTTPS/polling fork and
  stays idle when the carrier does not configure that flow. It provides neither
  IMS transport nor AML and cannot replace mandatory carrier push support.

RTT capabilities follow the stock TeleService overlay. Emergency routing stays
in the maintained Android telephony/Telecom stack and the selected radio/IMS
implementation; no new emergency-number list, local fallback heuristic,
location recipient or forced provisioning state is added. Existing GNSS/SUPL
and modem emergency-location paths keep their selected configuration; AML is
absent.

Checks, in order:

1. Build and check AIDL, VINTF, JNI linking, UID uniqueness, neverallows, seapp
   matching and installed permissions.
2. Run the IMS repository's read-only checker with `--iwlan qti`.
3. On the phone, test ordinary calls, SMS and Wi-Fi calling on both SIMs; keep
   emergency simulations separate from carrier-authorized observations.

Presence checks and host simulations do not establish registration, handover,
call audio or emergency/location delivery.
