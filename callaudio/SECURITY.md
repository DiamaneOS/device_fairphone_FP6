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
- `vsid` accepts the vendor's 32-bit bit pattern; the HAL keeps authority over
  valid session IDs and call states.
- `call_type` keeps the producer's text (including `UNKNOWN`) within a
  32-character ASCII token grammar; it is not mapped to a numeric enum.
- `crs_call` uses the HAL's literal `true` / `false`.
- Accepted messages are passed on verbatim and never logged.
- Limits: 256 characters per message; per-registration count and payload
  budgets, which include running work.
- Registration identity, PHONE_UID callbacks, ordering and audioserver
  recovery fences are required.

## Authority

- AudioFlinger (paired source change) reserves the vendor call-control keys
  and `all_call_states` queries for root, system, radio, audioserver and
  holders of `android.permission.DIAMANEOS_CONTROL_CALL_AUDIO`.
- The platform defines this signature/privileged permission; the bridge is its
  sole privileged allowlist entry.
- AudioFlinger also requires the primary user's unique bridge package and
  checks grants without a persistent permission cache.
- The bridge gains no routing, phone-state or network authority.
- Signing, package name and privileged placement select the bridge's dedicated
  SELinux domain.
- The boundary holds only while the release signing key stays private.
- Select the bridge, the frameworks/base permission and the frameworks/av
  changes together.

## SIM-mode changes

Each supported hardware slot has one serial worker.

- Disabling a slot immediately revokes its activation identity and its claim
  on queued parameters.
- Re-enabling registers only after the previous physical registration has
  returned.
- A coalesced reconnect task and a unique monitor listener avoid duplicate
  retries and accumulated listeners.

## Checks

Run after any bridge, AudioFlinger or HAL change.

- `FP6CallAudioHostTests`: parameter grammar, budgets, status relay and
  recovery.
- `FP6CallAudioAuthorizationTests`: on the phone with no active call, an
  ordinary app is refused the call keys and the call-state query.
- `FP6CallAudioLifecycleTests`: the actual Android worker with a local fake
  radio; no HAL, network or modem lookup.
- Ordinary calls and audio routing on the phone.
