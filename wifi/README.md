# Wi-Fi supplicant configuration

`wpa_supplicant_overlay.conf` and `p2p_supplicant_overlay.conf` are copied
unchanged from `/vendor/etc/wifi/` of the stock FP6 image FP6.QREL.16.100.0
(EU). SHA-256:

| File | SHA-256 |
| --- | --- |
| `wpa_supplicant_overlay.conf` | `cc7f31ca31417a4fe57a36f1b177b4de32c6ec70ce5780c1bd10a0be26d22029` |
| `p2p_supplicant_overlay.conf` | `355c62f3f28d994f39eb96d3a75a12d88d3f4ca32e556801da77ffda73d746cc` |

## Hardware address

## Options whose effect is not obvious

- `p2p_disabled=1` is in the station overlay only.
  - It keeps wlan0 from being registered as a P2P interface, which would make
    `addStaInterface` fail.
  - It does not turn off Wi-Fi Direct: the P2P overlay does not set it.
- `disable_scan_offload=1` turns off wpa_supplicant's own scheduled scanning.
  - Android's wificond does background scheduled scans separately, so this
    does not disable scan offload as a whole.
- `driver_param="no_rrm=1"` stops wpa_supplicant from registering for radio
  measurement (802.11k) requests.
  - The Qualcomm driver handles those itself and enables RRM by default.
- `p2p_no_group_iface=1` runs P2P groups on the P2P device interface instead
  of creating a separate group interface.

Still to be checked on the device, with SELinux enforcing and without the
directories created by hand during bring-up: supplicant socket and state
paths and labels; WPA2/WPA3 (and any supported enterprise) connect and
reconnect; background scanning, sleep/wake and roaming; MAC randomization;
Wi-Fi Direct and concurrency.
