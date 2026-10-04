# Call audio boundary

The bridge accepts the pinned QCRIL call-control keys `vsid`, `call_state`,
`call_type` and `crs_call`. Unknown or duplicate keys, malformed numeric values,
trailing separators, non-ASCII numeric input and oversized messages are rejected
before queuing or calling AudioFlinger. VSID accepts the vendor's 32-bit bit
pattern; the HAL retains authority over valid session IDs and call states.
CRS uses the HAL's literal `true` / `false` representation. Queries are limited
to `isCRSsupported=1`, its key-only form, and `all_call_states`.

Accepted messages are preserved verbatim and never logged. A 256-character
containment limit bounds each message; per-registration count and payload budgets
include running work. Registration identity, PHONE_UID callbacks, ordering and
audioserver recovery fences remain required. Signing plus package name and
privileged placement select the bridge's dedicated SELinux domain.

This boundary limits modem-influenced input to audio HAL parsers. It does not
establish that ordinary apps cannot invoke the same AudioFlinger call-control
keys. That platform boundary requires a separate native check and, if needed,
an audioserver-side authorization change. Native calls and routing still need
qualification after any bridge or HAL change.
