#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// ============================================================
// AGRIGUARD B6 + B7 TEST
// B6 = Reliability / Wi-Fi recovery / HTTPS retries
// B7 = Device health / heartbeat
//
// This sketch DOES NOT control irrigation.
// ============================================================

// ============================================================
// 1. WIFI CONFIGURATION
// ============================================================

const char* WIFI_SSID = "CirkitWifi";
const char* WIFI_PASSWORD = "";

// ============================================================
// 2. SUPABASE CONFIGURATION
// ============================================================

const char* TELEMETRY_ENDPOINT =
    "https://jxotmuxfctrovjsfhrmm.supabase.co/functions/v1/ingest-telemetry-v2";

const char* DEVICE_UID = "AGRIGUARD-CTRL-001";
const char* DEVICE_KEY = "DEVICE KEY HERE";

const char* FIRMWARE_VERSION = "0.1.0";

// ============================================================
// 3. B6 RELIABILITY SETTINGS
// ============================================================

const int MAX_POST_ATTEMPTS = 3;
const unsigned long HTTP_TIMEOUT_MS = 10000UL;
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000UL;

const bool AUTOMATED_WIFI_TEST = true;
const unsigned long WIFI_TEST_INTERVAL_MS = 120000UL;
const unsigned long WIFI_TEST_OFF_TIME_MS = 20000UL;

// ============================================================
// 4. B7 DEVICE HEALTH SETTINGS
// ============================================================

const unsigned long HEARTBEAT_INTERVAL_MS = 300000UL;

// ============================================================
// 5. DEVICE HEALTH VARIABLES
// ============================================================

bool cloudOnline = false;
unsigned long lastSuccessfulCloudTransmission = 0;
unsigned long consecutiveCloudFailures = 0;
unsigned long lastHeartbeatMillis = 0;
unsigned long lastWiFiReconnectAttempt = 0;

// ============================================================
// 6. AUTOMATED WIFI TEST VARIABLES
// ============================================================

bool wifiTestActive = false;
unsigned long wifiTestStartMillis = 0;
unsigned long lastWiFiTestMillis = 0;

// ============================================================
// 7. WIFI CONNECTION
// ============================================================

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.println();
    Serial.println("========================================");
    Serial.println("B6 - WIFI CONNECTION / RECOVERY");
    Serial.println("========================================");

    Serial.print("Connecting to: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttempt < 15000UL) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi connected.");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());
    } else {
        Serial.println("Wi-Fi connection failed.");
    }
}

// ============================================================
// 8. B6 WIFI MAINTENANCE
// ============================================================

void maintainWiFi() {
    if (wifiTestActive) return;
    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();

    if (now - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
        lastWiFiReconnectAttempt = now;

        Serial.println();
        Serial.println("[B6] Wi-Fi connection lost.");
        Serial.println("[B6] Attempting automatic Wi-Fi reconnection...");

        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long reconnectStart = millis();

        while (WiFi.status() != WL_CONNECTED &&
               millis() - reconnectStart < 8000UL) {
            delay(500);
            Serial.print(".");
        }

        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            Serial.println("[B6] Wi-Fi RECONNECTED successfully.");
            Serial.print("[B6] IP address: ");
            Serial.println(WiFi.localIP());
            Serial.print("[B6] RSSI: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        } else {
            Serial.println();
            Serial.println("[B6] Wi-Fi reconnection attempt FAILED.");
            Serial.println("[B6] Will automatically retry.");
        }
    }
}

// ============================================================
// 9. AUTOMATED WIFI FAILURE TEST
// ============================================================

void automatedWiFiTest() {
    if (!AUTOMATED_WIFI_TEST) return;

    unsigned long now = millis();

    if (!wifiTestActive &&
        WiFi.status() == WL_CONNECTED &&
        now - lastWiFiTestMillis >= WIFI_TEST_INTERVAL_MS) {

        Serial.println();
        Serial.println();
        Serial.println("################################################");
        Serial.println("# B6 AUTOMATED WIFI FAILURE TEST              #");
        Serial.println("################################################");
        Serial.println();
        Serial.println("[B6 TEST] Two minutes elapsed.");
        Serial.println("[B6 TEST] Intentionally switching Wi-Fi OFF.");
        Serial.println("[B6 TEST] Wi-Fi will remain OFF for 15 seconds.");

        WiFi.disconnect(true);
        wifiTestActive = true;
        wifiTestStartMillis = now;
        return;
    }

    if (wifiTestActive &&
        now - wifiTestStartMillis >= WIFI_TEST_OFF_TIME_MS) {

        Serial.println();
        Serial.println("[B6 TEST] 15-second Wi-Fi outage complete.");
        Serial.println("[B6 TEST] Turning Wi-Fi back ON.");
        Serial.println("[B6 TEST] Attempting Wi-Fi reconnection...");

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long reconnectStart = millis();

        while (WiFi.status() != WL_CONNECTED &&
               millis() - reconnectStart < 15000UL) {
            delay(500);
            Serial.print(".");
        }

        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("[B6 TEST] Wi-Fi reconnected successfully.");
            Serial.print("[B6 TEST] Recovered IP: ");
            Serial.println(WiFi.localIP());
            Serial.print("[B6 TEST] Recovered RSSI: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
            Serial.println("[B6 TEST] Wi-Fi recovery CONFIRMED.");
        } else {
            Serial.println("[B6 TEST] Wi-Fi reconnection FAILED.");
            Serial.println("[B6 TEST] B6 automatic recovery will continue retrying.");
        }

        wifiTestActive = false;
        lastWiFiTestMillis = millis();
        lastWiFiReconnectAttempt = millis();
    }
}

// ============================================================
// 10. UUID GENERATOR
// ============================================================

String generateUUID() {
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    uint32_t r4 = esp_random();

    char uuid[37];

    // RFC 4122 version-4 UUID:
    // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    snprintf(
        uuid,
        sizeof(uuid),
        "%08lX-%04lX-%04lX-%04lX-%08lX%04lX",
        (unsigned long)r1,
        (unsigned long)(r2 & 0xFFFF),
        (unsigned long)((r3 & 0x0FFF) | 0x4000),
        (unsigned long)((r4 & 0x3FFF) | 0x8000),
        (unsigned long)r3,
        (unsigned long)(r4 & 0xFFFF)
    );

    return String(uuid);
}

// ============================================================
// 11. NONCE GENERATOR
// ============================================================

String generateNonce() {
    char nonce[33];
    uint8_t randomBytes[16];

    for (int i = 0; i < 16; i++) {
        randomBytes[i] = random(0, 256);
    }

    for (int i = 0; i < 16; i++) {
        sprintf(&nonce[i * 2], "%02X", randomBytes[i]);
    }

    nonce[32] = '\0';
    return String(nonce);
}

// ============================================================
// 12. NTP
// ============================================================

bool synchronizeNTP() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("B1 - NTP TIME SYNCHRONIZATION");
    Serial.println("========================================");

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    struct tm timeinfo;

    if (!getLocalTime(&timeinfo, 10000)) {
        Serial.println("NTP synchronization failed.");
        return false;
    }

    time_t now;
    time(&now);

    Serial.println("NTP synchronization successful.");
    Serial.print("UTC time: ");
    Serial.println(asctime(gmtime(&now)));

    Serial.print("Unix timestamp (ms): ");
    Serial.println((unsigned long long)now * 1000ULL);

    return true;
}

// ============================================================
// 13. TELEMETRY TIMESTAMP
// ============================================================

String getCurrentUTCTimestamp() {
    time_t now;
    time(&now);

    struct tm utcTime;
    gmtime_r(&now, &utcTime);

    char timestamp[25];

    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utcTime);

    return String(timestamp);
}

// ============================================================
// 14. BUILD TELEMETRY
// ============================================================

String buildTelemetryPayload() {
    StaticJsonDocument<4096> doc;

    doc["device_uid"] = DEVICE_UID;
    doc["firmware_version"] = FIRMWARE_VERSION;

    String recordedAt = getCurrentUTCTimestamp();

    JsonArray sensorReadings = doc.createNestedArray("sensor_readings");

    JsonObject zone1 = sensorReadings.createNestedObject();
    zone1["zone_id"] = "df2c16c6-e565-4468-bcf8-cfce01963290";
    zone1["moisture_percent"] = 67.89;
    zone1["raw_value"] = 1778;
    zone1["temperature_c"] = 13.0;
    zone1["humidity_percent"] = 63.0;
    zone1["drying_rate"] = 7.96;
    zone1["recorded_at"] = recordedAt;

    JsonObject zone2 = sensorReadings.createNestedObject();
    zone2["zone_id"] = "086086c3-e376-45f1-b0cd-f6953e8e8403";
    zone2["moisture_percent"] = 56.78;
    zone2["raw_value"] = 1901;
    zone2["temperature_c"] = 13.0;
    zone2["humidity_percent"] = 63.0;
    zone2["drying_rate"] = 7.96;
    zone2["recorded_at"] = recordedAt;

    JsonObject zone3 = sensorReadings.createNestedObject();
    zone3["zone_id"] = "d25e5e40-3b87-4888-aade-9a7641702862";
    zone3["moisture_percent"] = 42.50;
    zone3["raw_value"] = 2100;
    zone3["temperature_c"] = 13.0;
    zone3["humidity_percent"] = 63.0;
    zone3["drying_rate"] = 7.96;
    zone3["recorded_at"] = recordedAt;

    JsonObject tank = doc.createNestedObject("tank_reading");
    tank["level_percent"] = 75.0;
    tank["volume_l"] = 3937.5;
    tank["distance_cm"] = 45.0;
    tank["recorded_at"] = recordedAt;

    JsonObject system = doc.createNestedObject("system_status");
    system["status"] = cloudOnline ? "online" : "offline";
    system["message"] = "Agriguard B6/B7 test";
    system["battery_percent"] = 100;
    system["wifi_rssi"] = WiFi.RSSI();
    system["recorded_at"] = recordedAt;

    doc["heartbeat"] = true;

    JsonObject health = doc.createNestedObject("device_health");
    health["firmware_version"] = FIRMWARE_VERSION;
    health["uptime_seconds"] = millis() / 1000UL;
    health["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    health["wifi_rssi"] = WiFi.RSSI();
    health["cloud_online"] = cloudOnline;
    health["consecutive_failures"] = consecutiveCloudFailures;

    health["last_successful_transmission_seconds_ago"] =
        lastSuccessfulCloudTransmission == 0
            ? -1
            : ((millis() - lastSuccessfulCloudTransmission) / 1000UL);

    String payload;
    serializeJson(doc, payload);
    return payload;
}

// ============================================================
// 15. SEND TELEMETRY
// ============================================================

bool sendTelemetry() {
    String payload = buildTelemetryPayload();

    for (int attempt = 1; attempt <= MAX_POST_ATTEMPTS; attempt++) {
        Serial.println();
        Serial.println("----------------------------------------");
        Serial.print("[B6] HTTPS attempt ");
        Serial.print(attempt);
        Serial.print(" of ");
        Serial.println(MAX_POST_ATTEMPTS);

        String requestID = generateUUID();
        String nonce = generateNonce();

        time_t now;
        time(&now);

        unsigned long long timestamp =
            (unsigned long long)now * 1000ULL;

        Serial.print("Request ID: ");
        Serial.println(requestID);
        Serial.print("Nonce: ");
        Serial.println(nonce);
        Serial.print("Timestamp: ");
        Serial.println(timestamp);

        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;

        Serial.println("Opening HTTPS connection...");

        http.setConnectTimeout(HTTP_TIMEOUT_MS);
        http.setTimeout(HTTP_TIMEOUT_MS);

        if (!http.begin(client, TELEMETRY_ENDPOINT)) {
            Serial.println("[B6] HTTPS connection setup failed.");
            continue;
        }

        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-Device-UID", DEVICE_UID);
        http.addHeader("X-Device-Key", DEVICE_KEY);
        http.addHeader("X-Request-ID", requestID);
        http.addHeader("X-Device-Nonce", nonce);
        http.addHeader("X-Device-Timestamp", String(timestamp));

        Serial.println("Sending POST request...");

        int httpStatus = http.POST(payload);

        Serial.print("HTTP status: ");
        Serial.println(httpStatus);

        if (httpStatus >= 200 && httpStatus < 300) {
            String response = http.getString();

            Serial.println();
            Serial.println("Backend response:");
            Serial.println("----------------------------------------");
            Serial.println(response);
            Serial.println("----------------------------------------");

            http.end();

            cloudOnline = true;
            consecutiveCloudFailures = 0;
            lastSuccessfulCloudTransmission = millis();

            Serial.println();
            Serial.println("[B6] TELEMETRY TRANSMISSION SUCCESS");
            Serial.println("[B7] Cloud status: ONLINE");

            return true;
        }

        Serial.println();
        Serial.println("[B6] HTTPS transmission failed.");

        String errorResponse = http.getString();

        if (errorResponse.length() > 0) {
            Serial.println();
            Serial.println("Backend error response:");
            Serial.println("----------------------------------------");
            Serial.println(errorResponse);
            Serial.println("----------------------------------------");
        }

        if (httpStatus == -1) {
            Serial.println("[B6] Error: connection/transport failure.");
        }

        http.end();

        if (attempt < MAX_POST_ATTEMPTS) {
            unsigned long retryDelay = attempt * 2000UL;

            Serial.print("[B6] Retrying in ");
            Serial.print(retryDelay / 1000);
            Serial.println(" seconds...");

            delay(retryDelay);
        }
    }

    cloudOnline = false;
    consecutiveCloudFailures++;

    Serial.println();
    Serial.println("[B6] ALL HTTPS ATTEMPTS FAILED.");
    Serial.println("[B7] Cloud status: OFFLINE");
    Serial.print("[B7] Consecutive cloud failures: ");
    Serial.println(consecutiveCloudFailures);

    return false;
}

// ============================================================
// 16. SEND HEARTBEAT
// ============================================================

void sendHeartbeat() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("B7 - DEVICE HEARTBEAT");
    Serial.println("========================================");

    Serial.println("Preparing device health telemetry.");
    Serial.println();

    Serial.println("Device health:");
    Serial.println("----------------------------------------");

    Serial.print("Firmware: ");
    Serial.println(FIRMWARE_VERSION);

    Serial.print("Uptime: ");
    Serial.print(millis() / 1000UL);
    Serial.println(" seconds");

    Serial.print("Wi-Fi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");

    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());

    Serial.print("Cloud (last known state): ");
    Serial.println(cloudOnline ? "ONLINE" : "OFFLINE");

    Serial.print("Consecutive cloud failures: ");
    Serial.println(consecutiveCloudFailures);

    Serial.println("----------------------------------------");
    Serial.println();

    bool success = sendTelemetry();

    Serial.println();
    Serial.println("[B7] Current cloud status after heartbeat:");
    Serial.println(success ? "ONLINE" : "OFFLINE");

    if (success) {
        Serial.println("[B7] Heartbeat accepted by backend.");
    } else {
        Serial.println("[B7] Heartbeat was NOT accepted by backend.");
    }
}

// ============================================================
// 17. SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println();
    Serial.println("################################################");
    Serial.println("#                                              #");
    Serial.println("#        AGRIGUARD B6 + B7 TEST               #");
    Serial.println("#        RELIABILITY + DEVICE HEALTH          #");
    Serial.println("#                                              #");
    Serial.println("################################################");
    Serial.println();

    Serial.println("This sketch does NOT control irrigation.");
    Serial.println("It tests B6 reliability and B7 health.");
    Serial.println();

    Serial.println("LIVE BACKEND:");
    Serial.println(TELEMETRY_ENDPOINT);

    connectWiFi();
    synchronizeNTP();

    Serial.println();
    Serial.println("Sending initial B7 heartbeat...");
    sendHeartbeat();

    lastHeartbeatMillis = millis();
    lastWiFiTestMillis = millis();
}

// ============================================================
// 18. MAIN LOOP
// ============================================================

void loop() {
    unsigned long now = millis();

    maintainWiFi();
    automatedWiFiTest();

    if (WiFi.status() == WL_CONNECTED &&
        now - lastHeartbeatMillis >= HEARTBEAT_INTERVAL_MS) {

        lastHeartbeatMillis = now;
        sendHeartbeat();
    }

    delay(100);
}
