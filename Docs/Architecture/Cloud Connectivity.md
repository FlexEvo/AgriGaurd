# AgriGuard Cloud Connectivity Architecture

## Principle

Cloud connectivity is an enhancement layer, not a safety dependency.

```
                    AGRIGUARD ESP32
                         |
          +--------------+--------------+
          |                             |
          v                             v
   LOCAL CONTROL                 CLOUD CLIENT
          |                             |
   +------+------+              +-------+-------+
   |      |      |              |       |       |
 Soil   Tank   Valves          Wi-Fi   NTP   HTTPS
   |      |      |                      |
   +------+------+                      v
          |                     Supabase Edge Function
          |                              |
          |                     Atomic telemetry ingest
          |                              |
          v                              v
    SAFE IRRIGATION              Supabase telemetry
```

## Failure behavior

If Wi-Fi or the cloud service becomes unavailable:

1. Local soil monitoring continues.
2. Tank monitoring continues.
3. Valve/safety logic continues.
4. Cloud telemetry is marked offline.
5. The device retries according to the configured retry policy.
6. Reconnection does not require a device reboot.

## B6 retry policy

The current test configuration uses:

- maximum POST attempts: 3
- HTTP timeout: 10 seconds
- retry delays: 2 seconds, then 4 seconds
- automated Wi-Fi outage test
- automatic Wi-Fi reconnection

## B7 heartbeat

The heartbeat provides device-health information such as:

- firmware version
- uptime
- Wi-Fi state
- Wi-Fi RSSI
- cloud state
- consecutive cloud failures
- last successful cloud transmission

A successful heartbeat also updates the device's server-side last-seen information.

## Next milestone

After the corrected B6/B7 sketch receives a successful HTTP 2xx response and the corresponding Supabase rows are verified, B1-B7 can be marked as end-to-end verified.

The next major workstream is the farmer-facing profile/onboarding flow.
