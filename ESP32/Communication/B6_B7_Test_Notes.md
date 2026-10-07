# AgriGuard B6 + B7 Reliability and Device Health Test

## Purpose

This test sketch validates the cloud-communication reliability layer without controlling irrigation.

- **B1:** NTP time synchronization
- **B2:** Device configuration/credential structure
- **B3:** Request UUID and nonce generation
- **B4:** Telemetry payload structure
- **B5:** HTTPS client
- **B6:** HTTP retry/backoff and Wi-Fi recovery
- **B7:** Device heartbeat and cloud-health tracking

## Important safety boundary

The B6/B7 tester does **not** control valves or irrigation. Cloud connectivity must never become a single point of failure for the local irrigation controller.

## Fixes applied after HTTP 400 testing

### 1. Request ID UUID format

The previous generator produced:

`8-4-4-4-8`

which is not a valid UUID and was rejected by `ingest-telemetry-v2`.

The corrected generator produces an RFC 4122 version-4 UUID:

`8-4-4-4-12`

Example:

`9CF5019C-416A-4BD6-8A5B-49DF1BD6A123`

### 2. Telemetry timestamps

The backend accepts `recorded_at` timestamps only when they are valid ISO-8601 timestamps and within the configured five-minute freshness window.

The corrected telemetry now adds `recorded_at` to:

- every sensor reading
- the tank reading
- the system status object

Format:

`YYYY-MM-DDTHH:MM:SSZ`

### 3. HTTP error visibility

Non-2xx responses now print the response body returned by the Edge Function. This makes future API-contract failures directly diagnosable from Serial Monitor.

## Expected successful flow

```
ESP32
  |
  +-- Wi-Fi
  |
  +-- NTP time
  |
  +-- UUID + nonce
  |
  +-- telemetry JSON
  |
  +-- HTTPS POST
  v
ingest-telemetry-v2
  |
  +-- device authentication
  +-- request freshness
  +-- replay protection
  +-- payload validation
  +-- zone authorization
  v
ingest_telemetry_atomic()
  |
  +-- sensor_readings
  +-- tank_readings
  +-- system_status
  +-- devices.last_seen_at
  +-- device_request_nonces
```

## Security

Never commit:

- the real device API key
- Supabase service-role keys
- dashboard secrets
- private credentials

Use local configuration or a secure provisioning mechanism for real credentials.
