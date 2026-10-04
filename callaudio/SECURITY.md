# Call audio boundary

The bridge (`de.diamaneos.callaudio`) passes the radio daemon's in-call audio
parameters to the audio HAL. This boundary limits the modem-influenced input
that reaches the audio HAL's parsers.

## Accepted input

- Set requests: only the QCRIL call-control keys `vsid`, `call_state`,
  `call_type` and `crs_call`.
- Queries: only `isCRSsupported=1`, its key-only form, and `all_call_states`.
- Rejected before queuing or calling AudioFlinger: unknown or duplicate keys,
  malformed numeric values, trailing separators, non-ASCII numeric input and
  oversized messages.
- `vsid` accepts the vendor's 32-bit bit pattern, and the HAL keeps authority over
  valid session IDs and call states.
- The producer's textual call type (including `UNKNOWN`) is preserved within a
  32-character ASCII token grammar, not an invented numeric enum.
- CRS uses the HAL's literal `true` / `false`.
- Queries are limited to `isCRSsupported=1`, its key-only form, and
  `all_call_states`.
- Accepted messages are preserved verbatim and never logged. A 256-character
  containment limit bounds each message; per-registration count and payload
  budgets include running work.
- Registration identity, PHONE_UID callbacks, ordering and audioserver recovery
  fences remain required.
- Signing plus package name and privileged placement select the bridge's
  dedicated SELinux domain.

This boundary limits modem-influenced input to audio HAL parsers. The paired
AudioFlinger source change additionally reserves vendor call-control keys and
`all_call_states` queries for root, system, radio, audioserver and holders of the
bridge's signature-only `de.diamaneos.permission.CONTROL_CALL_AUDIO` permission.
The bridge defines and requests that permission; it gains no routing, phone-state
or network authority. Select the bridge and framework changes together.

## SIM-mode changes

Each supported hardware slot has one serial worker.
