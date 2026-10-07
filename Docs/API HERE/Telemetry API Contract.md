# AgriGuard Telemetry API Contract

## Endpoint

`POST /functions/v1/ingest-telemetry-v2`

The device authenticates using custom headers.

## Required headers

| Header | Purpose |
|---|---|
| `x-device-uid` | Provisioned device identifier |
| `x-device-key` | Private device credential |
| `x-request-id` | RFC 4122 UUID request identifier |
| `x-device-nonce` | Per-request replay-protection nonce |
| `x-device-timestamp` | Unix timestamp in milliseconds |

## Freshness

The Edge Function currently accepts authenticated request timestamps within approximately five minutes of server time.

## Telemetry structure

```json
{
  "heartbeat": true,
  "sensor_readings": [
    {
      "zone_id": "ZONE_UUID",
      "moisture_percent": 67.89,
      "raw_value": 1778,
      "temperature_c": 13.0,
      "humidity_percent": 63.0,
      "drying_rate": 7.96,
      "recorded_at": "2026-10-06T23:55:32Z"
    }
  ],
  "tank_reading": {
    "level_percent": 75.0,
    "volume_l": 3937.5,
    "distance_cm": 45.0,
    "recorded_at": "2026-10-06T23:55:32Z"
  },
  "system_status": {
    "status": "online",
    "message": "AgriGuard B6/B7 test",
    "battery_percent": 100,
    "wifi_rssi": -50,
    "recorded_at": "2026-10-06T23:55:32Z"
  }
}
```

## Accepted system states

- `online`
- `offline`
- `watering`
- `settling`
- `checking`
- `emergency_stop`
- `fault`

## Validation notes

- Sensor arrays may contain up to three readings per request.
- Zone IDs must be valid UUIDs and must belong to the authenticated device and its farm.
- Percentages must be between 0 and 100.
- Raw sensor values must be non-negative integers.
- Tank volume and distance must be non-negative.
- Wi-Fi RSSI must be zero or negative.
- `recorded_at` values must be valid and fresh.

## Response

Successful ingestion returns an `ok: true` response and a server-side receipt timestamp.

Failures are mapped to meaningful HTTP responses, including:

- `400` invalid payload
- `401` authentication/device state failure
- `403` unauthorized zone
- `409` replay detected
- `429` device rate limit exceeded

## Credential rule

Device credentials and farmer authentication are separate security domains.

The ESP32 device key must never be used as a farmer login credential and must never be committed to GitHub.
