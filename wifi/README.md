# Wi-Fi configuration

## Supplicant overlays

`wpa_supplicant_overlay.conf` and `p2p_supplicant_overlay.conf` come from
`/vendor/etc/wifi/` of the stock FP6 image.

| File | Stock SHA-256 | Here |
| --- | --- | --- |
| `wpa_supplicant_overlay.conf` | `cc7f31ca31417a4fe57a36f1b177b4de32c6ec70ce5780c1bd10a0be26d22029` | stock without `wowlan_triggers=magic_pkt` |
| `p2p_supplicant_overlay.conf` | `355c62f3f28d994f39eb96d3a75a12d88d3f4ca32e556801da77ffda73d746cc` | unchanged |

- `device.mk` installs them to `/vendor/etc/wifi/`, where wpa_supplicant looks
  for overlays.
- The base template, `wpa_supplicant.conf`, has one provider: the
  `wpa_supplicant.conf` module in `hardware/qcom/wlan/qcwcn/config`
  (DiamaneOS/hardware_qcom_wlan).
- The supplicant, its services and the permission file come from
  `vendor/diamaneos/config/wifi.mk`.
- `init.qcom.rc` creates `/data/vendor/wifi/wpa/sockets`.

## Hardware address

- The station's hardware address is the factory MAC from the traceability
  partition, as on stock.
- imeiprovd (`hardware/diamaneos/ims`) writes the driver's `wlan_mac.bin` at
  boot; ueventd serves it from `/mnt/vendor/wlan_mac/` (`boot/ueventd.rc`).
- The driver reads it only with `read_mac_addr_from_mac_file=1` in
  `WCNSS_qcom_cfg.ini` (stock value).
- Android randomises the address per network on top of it.

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
- No magic-packet wake-up: a LAN peer must not be able to wake the phone.
  - The station overlay sets no `wowlan_triggers`.
  - The generated vendor tree sets `gEnableWoW=2` in the driver's
    `WCNSS_qcom_cfg.ini`; the qcacld driver uses that value and ignores
    cfg80211 WoWLAN triggers.
  - Pattern wake-ups stay on.

## Hotspot, Wi-Fi Direct and Wi-Fi Aware

All three are on, as GrapheneOS has them on Pixels and the stock FP6 declares
them (`android.hardware.wifi.direct` and `android.hardware.wifi.aware`).

- **Hotspot:** AOSP's hostapd from `external/wpa_supplicant_8` (802.11ax, no
  driver command library; `BoardConfig.mk`).
  - It has its own init file, VINTF fragment and SELinux domain
    (`hal_wifi_hostapd_default`); no device rules.
  - Stock's hostapd, Qualcomm's older build of the same code, is not used.
  - The Wi-Fi HAL has the driver create the hotspot interface (wlan1).
  - As on GrapheneOS, Android's defaults apply: a new random address at each
    start, WPA2/WPA3 transition security, and the hotspot turns off after 10
    minutes without clients.
- **Wi-Fi Direct:** the supplicant runs P2P on the driver's p2p0
  (`p2p_supplicant_overlay.conf`).
  - The Wi-Fi overlay declares P2P MAC randomization, so Android has the
    supplicant use random device and group addresses.
- **Aware:** `wifi.aware.interface=wifi-aware0`, as on stock.
  - The driver creates its own NAN interface and expects the NAN commands
    there.
- The hotspot's and Wi-Fi Direct's random addresses depend on the driver
  accepting a new address.

## Stock overlay values left out

- 802.11be for the hotspot: the QCA6750 is an 802.11ax chip, and hostapd is
  built without be.
- Left out so that behaviour matches GrapheneOS on Pixels (Android's
  defaults):
  - DFS channels in automatic channel selection;
  - an Enhanced Open (OWE) hotspot;
  - changing the hotspot's country without a restart;
  - several Aware networks on one data interface;
  - stock's hotspot name ("Fairphone 6 AP"); Android names it AndroidAP with
    four random digits.
