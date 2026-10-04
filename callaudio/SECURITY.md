# Call audio boundary

The bridge accepts the pinned QCRIL call-control keys `vsid`, `call_state`,
`call_type` and `crs_call`. Unknown or duplicate keys, malformed numeric values,
trailing separators, non-ASCII numeric input and oversized messages are rejected
before queuing or calling AudioFlinger. VSID accepts the vendor's 32-bit bit
pattern; the HAL retains authority over valid session IDs and call states.
The producer's textual call type (including `UNKNOWN`) is preserved within a
32-character ASCII token grammar, rather than an invented numeric enum.
CRS uses the HAL's literal `true` / `false` representation. Queries are limited
to `isCRSsupported=1`, its key-only form, and `all_call_states`.

Accepted messages are preserved verbatim and never logged. A 256-character
containment limit bounds each message; per-registration count and payload budgets
include running work. Registration identity, PHONE_UID callbacks, ordering and
audioserver recovery fences remain required. Signing plus package name and
privileged placement select the bridge's dedicated SELinux domain.

This boundary limits modem-influenced input to audio HAL parsers. The paired
AudioFlinger source change additionally reserves vendor call-control keys and
`all_call_states` queries for root, system, radio, audioserver and holders of the
platform's `android.permission.DIAMANEOS_CONTROL_CALL_AUDIO` permission.
The platform defines this signature/privileged permission; the bridge is its sole
privileged allowlist entry. AudioFlinger also requires the primary user's unique
bridge package and checks grants without a persistent permission cache. It gains no routing, phone-state
or network authority. Select the bridge, frameworks/base permission and frameworks/av changes together.

Native denial tests and ordinary calls/routing still need qualification after
any bridge, AudioFlinger or HAL change. Release keys must be private; development
test keys are public fixtures and cannot establish a production trust boundary.
