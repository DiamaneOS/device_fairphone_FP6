# IMS and Wi-Fi calling integration

This candidate keeps the Qualcomm IMS/radio implementation and call-audio bridge.
`hardware/diamaneos/ims` replaces the missing data-connection broker. It serves
IMS/EIMS requests through Android networking and authenticates modem traffic by
the verified FP6 QRTR node (modem 0, AP 1). Its vendor UID is 2990, its AIDL API is
frozen at V1, and its runtime requires enforcing SELinux. Do not run a second DCM
publisher or infer a modem node from an incoming request.

The QTI IWLAN frontend binds all three framework interfaces: data service,
network service and qualified-network selection. Its certificate helper shares
the same stock-signed application UID/process, with no system UID or privileged
Android permission grants. The dedicated policy covers only the selected Binder,
HIDL and QRTR paths. Package-specific hidden-API access supports the stock Java
IPC classes without platform-signing the apps. The AOSP IWLAN alternative must
not be selected at the same time.

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
implementation. No new emergency-number list, local fallback heuristic, location
recipient or forced provisioning state is introduced. Existing GNSS/SUPL and
modem emergency-location paths retain their selected configuration; AML is absent.

Before a phone test, build and check AIDL, VINTF, JNI linking, UID uniqueness,
neverallows, seapp matching and installed permissions. Then use the IMS repository's
read-only checker with `--iwlan qti`. Presence checks and host simulations do not
establish registration, handover, call audio or emergency/location delivery.
Test ordinary calls, SMS and Wi-Fi calling on both SIMs; keep emergency simulations
separate from carrier-authorized observations.
