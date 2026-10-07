# B6 + B7 HTTP 400 Root-Cause and Verification Record

## Incident

The B6/B7 ESP32 tester could connect to Wi-Fi, synchronize NTP, establish HTTPS, and reach the Supabase Edge Function, but every telemetry POST returned HTTP 400.

The failure was therefore above the network/TLS layer.

## Root cause

The original generateUUID() function produced a request ID with this shape:

8-4-4-4-8

The Supabase Edge Function requires an RFC 4122 UUID:

8-4-4-4-12

Its validation rejects any request ID that does not match the UUID pattern.

### Corrected format

The firmware now generates:

8-4-4-4-12

with UUID version 4 and the correct variant bits.

## Telemetry timestamps

The backend currently permits omitted recorded_at values by falling back to server time, so the missing timestamps were **not the direct cause of the HTTP 400**.

They have nevertheless been added to the firmware payload so that device telemetry follows the documented API contract explicitly.

The format is:

YYYY-MM-DDTHH:MM:SSZ

## Diagnostic improvement

For every non-2xx response, the tester now prints the Edge Function response body. Future API-contract failures should therefore expose the backend's rejection reason directly in Serial Monitor.

## Verification sequence

After uploading the corrected tester:

1. Confirm Wi-Fi connection.
2. Confirm NTP synchronization.
3. Confirm each request ID is 8-4-4-4-12.
4. Confirm the nonce changes for every attempt.
5. Confirm HTTP returns a 2xx response.
6. Confirm the backend response contains ok: true.
7. Confirm the device reports cloud status as ONLINE.
8. Confirm consecutive cloud failures reset to zero.
9. Verify Supabase receives:
   - sensor readings
   - tank reading
   - system status
   - device last-seen update
   - request nonce
10. Repeat the B6 Wi-Fi outage test and confirm automatic recovery.

## Security reminder

The real device key must remain local and must never be committed to GitHub.

The repository tester intentionally contains a placeholder:

DEVICE KEY HERE

The live credential must be provisioned separately.

## Current status

- B1 NTP: implemented
- B2 configuration/credentials: implemented
- B3 UUID + nonce: corrected
- B4 telemetry model: implemented
- B5 HTTPS transport: implemented
- B6 retry + Wi-Fi recovery: implemented
- B7 heartbeat/device health: implemented
- Previous HTTP 400: root cause identified and firmware corrected
- Final live 2xx verification: **pending hardware upload with the real device credential**
