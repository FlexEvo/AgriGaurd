# Agriguard React dashboard

This is a first React implementation alongside the original HTML pages. The original pages are intentionally preserved.

## Run locally

The app uses browser ES modules (React and Supabase JS are loaded from esm.sh), so no Node build is required.

From the repository root:

```powershell
py -m http.server 5500
```

Open `http://localhost:5500/Dashboard/react-app/`.

## Current features

- Supabase Auth email/password sign-up and sign-in
- Farmer farm setup and three zone records
- Dashboard queries for the signed-in farmer's farm, zones, devices, sensor readings and tank readings
- 15-second dashboard refresh while the page is open
- Device freshness indicator based on the device's `last_seen_at`
- Loading, empty, and error states

## Supabase configuration and security

The frontend uses the project's **publishable** key. This key is designed to be public; it is not a service-role secret. Do not add a service-role key or a device API key to this frontend.

Database access still depends on correct RLS policies. If farm creation or queries return a permission error, review the policies and the Supabase Auth email-confirmation setting. Do not solve this by disabling RLS.

## ESP32 telemetry

The device posts to:

`https://jxotmuxfctrovjsfhrmm.supabase.co/functions/v1/ingest-telemetry-v2`

The Edge Function authenticates with the provisioned device UID/key, a fresh timestamp, request UUID, and nonce. The browser dashboard never receives the device key. The controller UID currently registered in the database is `AGRIGUARD-CTRL-001`.

The ESP32 test sketch in `ESP32/Communication/sketch_tester.ino` currently contains a placeholder for the device key in the repository. Supply the provisioned key locally in Arduino IDE; never commit it to GitHub. The database stores only a hash, so the original key cannot be recovered from the hash.

## Known integration boundary

The existing prototype controller is attached to the prototype farm, which currently has no authenticated owner. A signed-in farmer's new farm will not automatically see that device's telemetry until the device is safely assigned to that farmer's farm and its zone UUIDs are aligned. Do not change device/farm ownership just to make the UI appear connected. Provision the device against the intended farm and zones first.

This version displays telemetry already stored in Supabase; it does not issue remote irrigation commands. Irrigation control and emergency-stop logic must remain local to the ESP32.
