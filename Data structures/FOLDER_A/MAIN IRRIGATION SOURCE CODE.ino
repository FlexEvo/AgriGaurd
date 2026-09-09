#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/*
============================================================
                    AGRIGUARD
             SMART IRRIGATION SYSTEM
============================================================

SYSTEM SECTIONS
------------------------------------------------------------
1. Pin configuration
2. Constants
3. Enumerations
4. Data structures
5. Global variables
6. Soil moisture system
7. Weather API
8. Water tank monitoring
9. Flow sensor
10. Water budgeting
11. Irrigation profile / learning
12. Irrigation decision engine
13. Irrigation state machine
14. Emergency stop
15. System setup
16. Main loop
============================================================
*/


// ============================================================
// 1. PIN CONFIGURATION
// ============================================================

// Soil sensors
#define SOIL_SENSOR_1 34
#define SOIL_SENSOR_2 35
#define SOIL_SENSOR_3 36

// Water tank ultrasonic sensor
#define TANK_TRIGGER_PIN 12
#define TANK_ECHO_PIN    14

// Irrigation valves for 3 zones
#define ZONE1_VALVE_PIN 26
#define ZONE2_VALVE_PIN 27
#define ZONE3_VALVE_PIN 33

// Flow sensor
#define FLOW_SENSOR_PIN 32

// EMERGENCY STOP
#define E_STOP_PIN 25
#define RESET_BUTTON_PIN 13


// ============================================================
// 2. SYSTEM CONSTANTS
// ============================================================

// ---------------- SOIL ----------------

const int SAMPLE_SIZE = 10;
const unsigned long SOIL_RECORD_INTERVAL = 30UL * 60UL * 1000UL;         // 30 minutes
const float WATER_LEVEL_LIMIT = 30.0;

// ---------------- WEATHER ----------------

const int DAILY_DATA = 48;
const unsigned long WEATHER_INTERVAL = 60UL * 60UL * 1000UL;         // 1 hour

// Rain probability above this value
const float RAIN_THRESHOLD = 60.0;

// ---------------- TANK ----------------

const unsigned long TANK_RECORD_INTERVAL = 60UL * 60UL * 1000UL;         // 1 hour
const float TANK_RESERVE_PERCENT = 30.0;

// ---------------- FLOW SENSOR ----------------

const float PULSES_PER_LITRE = 450.0;
const unsigned long FLOW_RATE_WINDOW = 5000UL;

// ---------------- IRRIGATION ----------------

const int MAX_IRRIGATION_CYCLES = 3;
const unsigned long SAFETY_TIMEOUT = 60UL * 60UL * 1000UL;         // 1 hour
const unsigned long SETTLING_TIME = 5UL * 60UL * 1000UL;           // 5 minutes

// ---------------- LEARNING ----------------

const int IRRIGATION_HISTORY_SIZE = 10;
const float DEMAND_HORIZON_HOURS = 24.0;

// ---------------- THRESHOLD COEFFICIENTS ----------------

const float TEMP_COEFFICIENT = 3.97;
const float HUMIDITY_COEFFICIENT = 5.97;
const float DRYING_RATE_COEFFICIENT = 7.96;

// ============================================================
// 3. ENUMERATIONS
// ============================================================

enum SoilType { CLAY, SANDY, LOAMY };
enum TankModel { JJ1, JJ2, JJ3, JJ4, JJ5, JJ6 };
enum IrrigationState { IDLE, WATERING, SETTLING, CHECKING, MAX_CYCLES_REACHED, EMERGENCY_STOP };


// ============================================================
// 4. DATA STRUCTURES
// ============================================================

// ------------------------------------------------------------
// SOIL ZONE
// ------------------------------------------------------------

struct Zone {
    int zoneID;
    SoilType soilType;
    float dryCalibration;
    float wetCalibration;
    float soilMoistureData[SAMPLE_SIZE];
    unsigned long timeStampData[SAMPLE_SIZE];
    int sampleCount;
    float soilPlaceHolder;
    float timePlaceHolder;
    float currentSoilMoisture;
    float newSoilReading;
    float avgSoilMoisture;
    float dryingRate;
    bool valveStatus;
    int sensorPin;
    int valvePin;
};

// ------------------------------------------------------------
// WEATHER DATA
// ------------------------------------------------------------

struct WeatherData {
    String timeStamp;
    float temperature;
    float humidity;
    float rainProbability;
};

// ------------------------------------------------------------
// TANK PROFILE
// ------------------------------------------------------------

struct TankProfile {
    String manufacturer;
    int volume;          // Litres
    float height;        // cm
    float diameter;      // cm
};

// ------------------------------------------------------------
// CURRENT WATER TANK
// ------------------------------------------------------------

struct WaterTank {
    TankModel model;
    float echoTime;
    float distance;
    float waterHeight;
    float waterPercentage;
    float volume;
};

// ------------------------------------------------------------
// IRRIGATION PROFILE
// ------------------------------------------------------------

struct IrrigationProfile {
    int zoneID;
    float preWateringMoisture;
    float postWateringMoisture;
    float nextMoisture;
    float waterSpent;
    float litresPerMoisture[IRRIGATION_HISTORY_SIZE];
    float dryingRates[IRRIGATION_HISTORY_SIZE];
    unsigned long irrigationDurations[IRRIGATION_HISTORY_SIZE];
    float waterUsedHistory[IRRIGATION_HISTORY_SIZE];
    float averageLitresPerPercentage;
    float averageDryingRate;
    float averageWaterPerCycle;
    float averageCycleDuration;
    int irrigationCycles;
    float minTemperature;
    float maxTemperature;
    float maximumHumidity;
    float maximumRainProbability;
};

// ------------------------------------------------------------
// WATER BUDGET
// ------------------------------------------------------------

struct WaterBudget {
    float tankCapacity;
    float currentWater;
    float reservedWater;
    float usableWater;
    float predictedDemand;
};


// ============================================================
// 5. GLOBAL VARIABLES
// ============================================================

Zone zones[3];
WeatherData weather[DAILY_DATA];
WaterTank waterTank;
WaterBudget waterBudget;
IrrigationProfile irrigationProfile;

// ------------------------------------------------------------
// TANK PROFILES
// ------------------------------------------------------------

//Different JOJO tank sizes
TankProfile tankProfiles[] = {
    {"JoJo", 2000, 145.0, 142.0},
    {"JoJo", 5250, 225.5, 182.0},
    {"JoJo", 5500, 224.0, 190.0},
    {"JoJo", 10000, 315.0, 220.0},
    {"JoJo", 15000, 326.0, 260.0},
    {"JoJo", 20000, 427.0, 260.0}
};

// ------------------------------------------------------------
// TIMERS
// ------------------------------------------------------------

unsigned long soilTimer = 0;
unsigned long weatherTimer = 0;
unsigned long tankTimer = 0;

// ------------------------------------------------------------
// WEATHER STATUS
// ------------------------------------------------------------

bool weatherAvailable = false;
int currentWeatherIndex = 0;

// ------------------------------------------------------------
// FLOW SENSOR VARIABLES
// ------------------------------------------------------------

volatile unsigned long flowPulses = 0;
unsigned long flowWindowPulses = 0;
unsigned long flowWindowStart = 0;
float currentFlowRate = 0.0;

// ------------------------------------------------------------
// IRRIGATION VARIABLES
// ------------------------------------------------------------

IrrigationState irrigationState = IDLE;
unsigned long irrigationStartTime = 0;
unsigned long settlingStartTime = 0;
int irrigationCycles = 0;
float requiredWater = 0.0;

// ------------------------------------------------------------
// EMERGENCY STOP
// ------------------------------------------------------------

bool emergencyStopActive = false;


// ============================================================
// 6. SOIL MOISTURE SYSTEM
// ============================================================

float convertRawSoilToPercentage(Zone &zone, float rawValue) {
    float moisture = 0;
    if (zone.dryCalibration == zone.wetCalibration) {
        return 0;
    }
    moisture = (zone.dryCalibration - rawValue) / (zone.dryCalibration - zone.wetCalibration) * 100.0;
    if (moisture > 100) moisture = 100;
    if (moisture < 0) moisture = 0;
    return moisture;
}

// ------------------------------------------------------------
// RECORD SOIL SENSOR
// ------------------------------------------------------------

void recordSoilData(Zone &zone) {
    if (millis() - soilTimer < SOIL_RECORD_INTERVAL) {
        return;
    }
    soilTimer = millis();
    float rawValue = analogRead(zone.sensorPin);
    float moisture = convertRawSoilToPercentage(zone, rawValue);
    zone.soilPlaceHolder = rawValue;
    zone.timePlaceHolder = millis();
    zone.currentSoilMoisture = moisture;
    
    // Store historical reading
    if (zone.sampleCount < SAMPLE_SIZE) {
        int index = zone.sampleCount;
        zone.soilMoistureData[index] = moisture;
        zone.timeStampData[index] = millis() / 60000UL;       //Converts time to minutes  
        zone.sampleCount++;
    } else {
        // Shift old readings out
        for (int a = 0; a < SAMPLE_SIZE - 1; a++) {
            zone.soilMoistureData[a] = zone.soilMoistureData[a + 1];
            zone.timeStampData[a] = zone.timeStampData[a + 1];
        }
        zone.soilMoistureData[SAMPLE_SIZE - 1] = moisture;
        zone.timeStampData[SAMPLE_SIZE - 1] = millis() / 60000UL;
    }
    
    Serial.println();
    Serial.println("===== SOIL SENSOR =====");
    Serial.print("Zone: ");
    Serial.println(zone.zoneID);
    Serial.print("Raw: ");
    Serial.println(rawValue);
    Serial.print("Moisture: ");
    Serial.print(moisture);
    Serial.println("%");
    Serial.print("Samples: ");
    Serial.println(zone.sampleCount);
}

// ------------------------------------------------------------
// CALCULATE AVERAGE SOIL MOISTURE
// ------------------------------------------------------------

float getAverageSoilMoisture(Zone &zone) {
    if (zone.sampleCount == 0) {
        return 0;
    }
    float total = 0;
    for (int i = 0; i < zone.sampleCount; i++) {
        total += zone.soilMoistureData[i];
    }
    return total / zone.sampleCount;
}

// ------------------------------------------------------------
// CALCULATE DRYING RATE
// ------------------------------------------------------------

float getDryingRate(Zone &zone) {
    if (zone.sampleCount < 2) {
        return 0;
    }
    int last = zone.sampleCount - 1;
    float moistureChange = zone.soilMoistureData[0] - zone.soilMoistureData[last];
    float timeChange = zone.timeStampData[last] - zone.timeStampData[0];
    if (timeChange <= 0) {
        return 0;
    }
    zone.dryingRate = abs(moistureChange / timeChange);
    return zone.dryingRate;
}

// ------------------------------------------------------------
// GET NEW SOIL READING
// Used after irrigation
// ------------------------------------------------------------

void getNewSoilReading(Zone &zone) {
    float rawValue = analogRead(zone.sensorPin);
    zone.newSoilReading = convertRawSoilToPercentage(zone, rawValue);
    zone.currentSoilMoisture = zone.newSoilReading;
    Serial.print("New soil moisture: ");
    Serial.print(zone.newSoilReading);
    Serial.println("%");
}

// ============================================================
// 7. WEATHER API
// ============================================================

// ------------------------------------------------------------
// WiFi credentials
// ------------------------------------------------------------

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// ------------------------------------------------------------
// Agriguard location
// ------------------------------------------------------------

const float LAT = -29.35080374459709;
const float LON = 27.601807742259524;

// ------------------------------------------------------------
// CONNECT WIFI
// ------------------------------------------------------------

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected.");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi connection failed.");
    }
}

// ------------------------------------------------------------
// BUILD WEATHER API URL
// ------------------------------------------------------------

//Connection String
String buildWeatherURL() {
    String API_URL = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LAT) + "&longitude=" + String(LON) + 
    "&hourly=temperature_2m,relative_humidity_2m,precipitation_probability,precipitation&forecast_days=2&timezone=auto";
    
    return API_URL;
}

// ------------------------------------------------------------
// GET WEATHER DATA
// ------------------------------------------------------------

void getWeatherData() {
    if (WiFi.status() != WL_CONNECTED) {
        weatherAvailable = false;
        return;
    }
    HTTPClient http;          //This object will enable the system to make HTTP requests
    String url = buildWeatherURL();
    Serial.println();
    Serial.println("===== WEATHER API =====");
    Serial.println(url);
    http.setConnectTimeout(5000);  // 5s to establish connection
    http.setTimeout(10000);         // 10 sec waiting for data
    http.begin(url);
    int httpCode = http.GET();        //Creates a connection to send the HTTP request
    
    //Confirms if data was recieved
    if (httpCode == HTTP_CODE_OK) {
        String jsonData = http.getString();      //Reads data 
        DynamicJsonDocument doc(16384);
        DeserializationError error = deserializeJson(doc, jsonData);
        if (error) {
            Serial.print("JSON error: ");
            Serial.println(error.c_str());
            weatherAvailable = false;
            http.end();
            return;
        }
        JsonObject jsonOb = doc.as<JsonObject>();
        for (int a = 0; a < DAILY_DATA; a++) {
            weather[a].timeStamp = jsonOb["hourly"]["time"][a].as<String>();
            weather[a].temperature = jsonOb["hourly"]["temperature_2m"][a].as<float>();
            weather[a].humidity = jsonOb["hourly"]["relative_humidity_2m"][a].as<float>();
            weather[a].rainProbability = jsonOb["hourly"]["precipitation_probability"][a].as<float>();
        }
        currentWeatherIndex = 0;
        weatherAvailable = true;
        //Other random stuff
        Serial.println("Weather data updated.");
        Serial.print("Temperature: ");
        Serial.println(weather[0].temperature);
        Serial.print("Humidity: ");
        Serial.println(weather[0].humidity);
        Serial.print("Rain probability: ");
        Serial.println(weather[0].rainProbability);
    } else {
        Serial.print("Weather HTTP error: ");
        Serial.println(httpCode);
        weatherAvailable = false;
    }
    http.end();
}

// ------------------------------------------------------------
// CURRENT WEATHER
// ------------------------------------------------------------

WeatherData getCurrentWeather() {
    return weather[currentWeatherIndex];
}

// ============================================================
// 8. WATER TANK SYSTEM
// ============================================================

// ------------------------------------------------------------
// RECORD ULTRASONIC DATA
// ------------------------------------------------------------

// Read ultrasonic sensor
void recordTankData() {
    digitalWrite(TANK_TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TANK_TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TANK_TRIGGER_PIN, LOW);
    waterTank.echoTime = pulseIn(TANK_ECHO_PIN, HIGH, 30000);
}

// ------------------------------------------------------------
// CONVERT DISTANCE
// ------------------------------------------------------------

// Convert echo time into distance
void convertTankDistance() {
    waterTank.distance = (waterTank.echoTime * 0.0343) / 2.0;       //The 0.0343 is the speed of sound
}

// ------------------------------------------------------------
// CALCULATE WATER LEVEL
// ------------------------------------------------------------

void calculateWaterLevel() {
    TankProfile currentTank = tankProfiles[waterTank.model];
    waterTank.waterHeight = currentTank.height - waterTank.distance;
    // Prevent invalid values
    if (waterTank.waterHeight < 0) {
        waterTank.waterHeight = 0;
    }
    if (waterTank.waterHeight > currentTank.height) {
        waterTank.waterHeight = currentTank.height;
    }
    waterTank.waterPercentage = (waterTank.waterHeight / currentTank.height) * 100.0;
    if (waterTank.waterPercentage > 100) waterTank.waterPercentage = 100;
    if (waterTank.waterPercentage < 0) waterTank.waterPercentage = 0;
    waterTank.volume = (waterTank.waterPercentage / 100.0) * currentTank.volume;
}

// ------------------------------------------------------------
// UPDATE TANK
// ------------------------------------------------------------

void updateTank() {
    if (millis() - tankTimer < TANK_RECORD_INTERVAL) {
        return;
    }
    tankTimer = millis();
    recordTankData();
    convertTankDistance();
    calculateWaterLevel();
    TankProfile currentTank = tankProfiles[waterTank.model];
    Serial.println();
    Serial.println("===== WATER TANK =====");
    Serial.print("Tank capacity: ");
    Serial.print(currentTank.volume);
    Serial.println(" L");
    Serial.print("Distance: ");
    Serial.print(waterTank.distance);
    Serial.println(" cm");
    Serial.print("Water height: ");
    Serial.print(waterTank.waterHeight);
    Serial.println(" cm");
    Serial.print("Water level: ");
    Serial.print(waterTank.waterPercentage);
    Serial.println("%");
    Serial.print("Estimated water: ");
    Serial.print(waterTank.volume);
    Serial.println(" L");
}

// ============================================================
// 9. FLOW SENSOR
// ============================================================

// Interrupt function
void IRAM_ATTR countFlowPulse() {
    flowPulses++;
}

// ------------------------------------------------------------
// GET TOTAL WATER VOLUME
// ------------------------------------------------------------

// Stuff from flow sensor
float getWaterVolume() {
    noInterrupts();        //This pauses/holds interrupts to count pulses
    unsigned long pulses = flowPulses;
    interrupts();
    float convPulse = 0;
    convPulse = pulses / PULSES_PER_LITRE;      //Converts pulses into litres
    return convPulse;
}

// ------------------------------------------------------------
// GET FLOW RATE
// ------------------------------------------------------------

// Calculate current flow rate
float getFlowRate() {
    unsigned long now = millis();
    //Determines the time difference between start time and recording time
    if (now - flowWindowStart < FLOW_RATE_WINDOW) {
        return currentFlowRate;
    }
    noInterrupts();
    unsigned long currentPulses = flowPulses;
    interrupts();
    unsigned long pulseDifference = currentPulses - flowWindowPulses;
    float elapsedSeconds = (now - flowWindowStart) / 1000.0;
    if (elapsedSeconds > 0) {
        float pulsesPerSecond = pulseDifference / elapsedSeconds;
        float litresPerSecond = pulsesPerSecond / PULSES_PER_LITRE;
        currentFlowRate = litresPerSecond * 60.0;
    }
    flowWindowPulses = currentPulses;
    flowWindowStart = now;
    return currentFlowRate;
}

// ============================================================
// 10. WATER BUDGET
// ============================================================

void updateWaterBudget() {
    TankProfile currentTank = tankProfiles[waterTank.model];
    waterBudget.tankCapacity = currentTank.volume;
    waterBudget.currentWater = waterTank.volume;
    waterBudget.reservedWater = (TANK_RESERVE_PERCENT / 100.0) * waterBudget.tankCapacity;
    waterBudget.usableWater = waterBudget.currentWater - waterBudget.reservedWater;
    if (waterBudget.usableWater < 0) {
        waterBudget.usableWater = 0;
    }
}

// ------------------------------------------------------------
// CHECK AVAILABLE WATER
// ------------------------------------------------------------

bool enoughWater(float required) {
    updateWaterBudget();
    return (waterBudget.usableWater >= required);
}

// ============================================================
// 11. IRRIGATION PROFILE / LEARNING
// ============================================================

// ------------------------------------------------------------
// UPDATE PROFILE AVERAGES
// ------------------------------------------------------------

void updateProfileAverages() {
    int count = irrigationProfile.irrigationCycles;
    if (count <= 0) {
        return;
    }
    if (count > IRRIGATION_HISTORY_SIZE) {
        count = IRRIGATION_HISTORY_SIZE;
    }
    float litresTotal = 0;
    float dryingTotal = 0;
    float waterTotal = 0;
    float durationTotal = 0;
    int litresCount = 0;
    int dryingCount = 0;
    for (int i = 0; i < count; i++) {
        if (irrigationProfile.litresPerMoisture[i] > 0) {
            litresTotal += irrigationProfile.litresPerMoisture[i];
            litresCount++;
        }
        if (irrigationProfile.dryingRates[i] > 0) {
            dryingTotal += irrigationProfile.dryingRates[i];
            dryingCount++;
        }
        waterTotal += irrigationProfile.waterUsedHistory[i];
        durationTotal += irrigationProfile.irrigationDurations[i];
    }
    if (litresCount > 0) {
        irrigationProfile.averageLitresPerPercentage = litresTotal / litresCount;
    }
    if (dryingCount > 0) {
        irrigationProfile.averageDryingRate = dryingTotal / dryingCount;
    }
    irrigationProfile.averageWaterPerCycle = waterTotal / count;
    irrigationProfile.averageCycleDuration = durationTotal / count;
}

// ------------------------------------------------------------
// CALCULATE PREVIOUS CYCLE
// ------------------------------------------------------------

void calculatePreviousCycle() {
    int index = irrigationProfile.irrigationCycles;
    if (index >= IRRIGATION_HISTORY_SIZE) {
        index = IRRIGATION_HISTORY_SIZE - 1;
    }
    float moistureIncrease = irrigationProfile.postWateringMoisture - irrigationProfile.preWateringMoisture;
    float moistureLoss = irrigationProfile.postWateringMoisture - irrigationProfile.nextMoisture;
    
    // Litres per moisture percentage
    if (moistureIncrease > 0 && irrigationProfile.waterSpent > 0) {
        irrigationProfile.litresPerMoisture[index] = irrigationProfile.waterSpent / moistureIncrease;
    }
    // Drying rate
    if (moistureLoss > 0 && DEMAND_HORIZON_HOURS > 0) {
        irrigationProfile.dryingRates[index] = moistureLoss / DEMAND_HORIZON_HOURS;
    }
    // Water used
    irrigationProfile.waterUsedHistory[index] = irrigationProfile.waterSpent;
    // Duration
    irrigationProfile.irrigationDurations[index] = millis() - irrigationStartTime;
    updateProfileAverages();
}

// ============================================================
// 12. IRRIGATION DECISION ENGINE
// ============================================================

// ------------------------------------------------------------
// GET START THRESHOLD
// ------------------------------------------------------------

float getStartThreshold(Zone &zone) {
    float baseThreshold = 0;
    float dryingRate = getDryingRate(zone);
    float tempEffect = 0;
    float humidityEffect = 0;
    float dryingEffect = 0;
    float threshold = 0;

    switch (zone.soilType) {
        case CLAY: 
        baseThreshold = 47;
         break;

        case SANDY: 
        baseThreshold = 27;
         break;

        case LOAMY: 
        baseThreshold = 40;
         break;
    }

    if (weatherAvailable) {
        WeatherData current = getCurrentWeather();
        // Hotter than 20°C increases irrigation need
        tempEffect = constrain((current.temperature - 20.0) / 15.0, -1.0, 1.0);
        humidityEffect = constrain((60.0 - current.humidity) / 35.0, -1.0, 1.0);
    }
    
     dryingEffect = constrain((dryingRate - 0.5) / 1.5, -1.0, 1.0);

    threshold = baseThreshold + (tempEffect * TEMP_COEFFICIENT) + (humidityEffect * HUMIDITY_COEFFICIENT) + (dryingEffect * DRYING_RATE_COEFFICIENT);

    if(threshold > 100)
    threshold = 100;

    if(threshold < 0)
    threshold = 0;

    return threshold;
}

// ------------------------------------------------------------
// GET STOP THRESHOLD
// ------------------------------------------------------------

float getStopThreshold(Zone &zone) {
    float baseThreshold = 0;
    switch (zone.soilType) {
        case CLAY: baseThreshold = 70; break;
        case SANDY: baseThreshold = 48; break;
        case LOAMY: baseThreshold = 56; break;
    }
    float temperatureEffect = 0;
    float humidityEffect = 0;
    float dryingEffect = 0;
    if (weatherAvailable) {
        WeatherData current = getCurrentWeather();
        temperatureEffect = constrain((current.temperature - 20.0) / 15.0, -1.0, 1.0);
        humidityEffect = constrain((60.0 - current.humidity) / 35.0, -1.0, 1.0);
    }
    float dryingRate = getDryingRate(zone);
    dryingEffect = constrain((dryingRate - 0.5) / 1.5, -1.0, 1.0);
    float threshold = baseThreshold + (temperatureEffect * TEMP_COEFFICIENT) + (humidityEffect * HUMIDITY_COEFFICIENT) + (dryingEffect * DRYING_RATE_COEFFICIENT);
    return constrain(threshold, 0.0, 100.0);
}

// ------------------------------------------------------------
// DETERMINE IF IRRIGATION SHOULD START
// ------------------------------------------------------------

bool shouldStartIrrigation(Zone &zone) {
    if (zone.sampleCount < SAMPLE_SIZE) {
        Serial.println("Waiting for soil history.");
        return false;
    }
    float threshold = getStartThreshold(zone);
    float moisture = zone.currentSoilMoisture;
    
    // Rain protection
    if (weatherAvailable) {
        float rain = weather[currentWeatherIndex].rainProbability;
        if (rain >= RAIN_THRESHOLD) {
            Serial.println("Rain expected. Irrigation postponed.");
            return false;
        }
    }
    // Water protection
    if (waterTank.waterPercentage <= WATER_LEVEL_LIMIT) {
        Serial.println("Tank below minimum level.");
        return false;
    }
    // Soil decision
    Serial.print("Start threshold: ");
    Serial.println(threshold);
    Serial.print("Current moisture: ");
    Serial.println(moisture);
    if (moisture < threshold) {
        return true;
    }
    return false;
}

// ============================================================
// 13. IRRIGATION PROFILE CALCULATIONS
// ============================================================

// ------------------------------------------------------------
// MOISTURE DEFICIT
// ------------------------------------------------------------

float getMoistureDeficit(Zone &zone) {
    float target = getStopThreshold(zone);
    float deficit = target - zone.currentSoilMoisture;
    if (deficit < 0) {
        deficit = 0;
    }
    return deficit;
}

// ------------------------------------------------------------
// REQUIRED WATER
// ------------------------------------------------------------

float getRequiredWater(Zone &zone) {
    float deficit = getMoistureDeficit(zone);
    // If there isn't enough historical data, use a temporary default.
    if (irrigationProfile.averageLitresPerPercentage <= 0) {
        /*
        TEMPORARY VALUE
        Replace after real calibration.
        */
        return deficit * 2.0;
    }
    return deficit * irrigationProfile.averageLitresPerPercentage;
}

// ------------------------------------------------------------
// PREDICT 24-HOUR WATER DEMAND
// ------------------------------------------------------------

float predictWaterDemand(Zone &zone) {
    float currentDeficit = getMoistureDeficit(zone);
    float expectedLoss = irrigationProfile.averageDryingRate * DEMAND_HORIZON_HOURS;
    float totalRequirement = currentDeficit + expectedLoss;
    if (irrigationProfile.averageLitresPerPercentage <= 0) {
        return 0;
    }
    return totalRequirement * irrigationProfile.averageLitresPerPercentage;
}

// ============================================================
// 14. IRRIGATION CONTROL
// ============================================================

// ------------------------------------------------------------
// RESET FLOW SENSOR
// ------------------------------------------------------------

void resetFlowMeasurement() {
    noInterrupts();
    flowPulses = 0;
    interrupts();
    flowWindowPulses = 0;
    flowWindowStart = millis();
    currentFlowRate = 0;
}

// ------------------------------------------------------------
// START IRRIGATION
// ------------------------------------------------------------

void startIrrigation(Zone &zone) {
    if (emergencyStopActive) {
        return;
    }
    if (waterTank.waterPercentage <= WATER_LEVEL_LIMIT) {
        Serial.println("Cannot irrigate: tank too low.");
        return;
    }
    resetFlowMeasurement();
    requiredWater = getRequiredWater(zone);
    if (requiredWater <= 0) {
        Serial.println("No water required.");
        return;
    }
    if (!enoughWater(requiredWater)) {
        Serial.println("Insufficient usable water.");
        return;
    }
    irrigationProfile.preWateringMoisture = zone.currentSoilMoisture;
    irrigationStartTime = millis();
    digitalWrite(zone.valvePin, HIGH);
    zone.valveStatus = true;
    irrigationState = WATERING;
    Serial.println();
    Serial.println("============================");
    Serial.println("IRRIGATION STARTED");
    Serial.print("Zone: ");
    Serial.println(zone.zoneID);
    Serial.print("Required water: ");
    Serial.print(requiredWater);
    Serial.println(" L");
    Serial.print("Moisture before: ");
    Serial.println(zone.currentSoilMoisture);
}

// ------------------------------------------------------------
// STOP IRRIGATION
// ------------------------------------------------------------

void stopIrrigation(Zone &zone) {
    digitalWrite(zone.valvePin, LOW);
    zone.valveStatus = false;
    irrigationProfile.waterSpent = getWaterVolume();
    unsigned long duration = millis() - irrigationStartTime;
    irrigationProfile.irrigationDurations[irrigationCycles] = duration;
    Serial.println();
    Serial.println("============================");
    Serial.println("IRRIGATION FINISHED");
    Serial.print("Zone: ");
    Serial.println(zone.zoneID);
    Serial.print("Water used: ");
    Serial.print(irrigationProfile.waterSpent);
    Serial.println(" L");
    Serial.print("Duration: ");
    Serial.print(duration / 60000.0);
    Serial.println(" minutes");
    settlingStartTime = millis();
    irrigationState = SETTLING;
}

// ------------------------------------------------------------
// UPDATE WATERING
// ------------------------------------------------------------

void updateWatering(Zone &zone) {
    if (emergencyStopActive) {
        return;
    }
    getFlowRate();
    float waterDelivered = getWaterVolume();
    // Tank safety
    if (waterTank.waterPercentage <= WATER_LEVEL_LIMIT) {
        Serial.println("Tank too low!");
        stopIrrigation(zone);
        return;
    }
    // Required water reached
    if (waterDelivered >= requiredWater) {
        stopIrrigation(zone);
        return;
    }
    // Safety timeout
    if (millis() - irrigationStartTime > SAFETY_TIMEOUT) {
        Serial.println("IRRIGATION SAFETY TIMEOUT!");
        stopIrrigation(zone);
        return;
    }
}

// ------------------------------------------------------------
// UPDATE SETTLING
// ------------------------------------------------------------

void updateSettling(Zone &zone) {
    if (millis() - settlingStartTime < SETTLING_TIME) {
        return;
    }
    Serial.println("Settling period complete.");
    getNewSoilReading(zone);
    irrigationProfile.postWateringMoisture = zone.currentSoilMoisture;
    irrigationProfile.irrigationCycles++;
    if (irrigationProfile.irrigationCycles > IRRIGATION_HISTORY_SIZE) {
        irrigationProfile.irrigationCycles = IRRIGATION_HISTORY_SIZE;
    }
    Serial.print("Post irrigation moisture: ");
    Serial.println(irrigationProfile.postWateringMoisture);
    // Save learning data
    if (irrigationProfile.irrigationCycles > 1) {
        irrigationProfile.nextMoisture = zone.currentSoilMoisture;
        calculatePreviousCycle();
    }
    irrigationState = IDLE;
}


// ============================================================
// EMERGENCY STOP FUNCTION
// ============================================================

void emergencyStop() {
    // E-stop pressed
    if (digitalRead(E_STOP_PIN) == HIGH) {
        emergencyStopActive = true;
        // Immediately shut down irrigation
        digitalWrite(ZONE1_VALVE_PIN, LOW);
        digitalWrite(ZONE2_VALVE_PIN, LOW);
        digitalWrite(ZONE3_VALVE_PIN, LOW);
        // Update zone status
        for (int i = 0; i < 3; i++) {
            zones[i].valveStatus = false;
        }
        // Change system state
        irrigationState = EMERGENCY_STOP;
        Serial.println("!!! EMERGENCY STOP ACTIVE !!!");
    }
}

// ------------------------------------------------------------
// EMERGENCY STATE
// ------------------------------------------------------------

void handleEmergencyStop() {
    digitalWrite(ZONE1_VALVE_PIN, LOW);
    digitalWrite(ZONE2_VALVE_PIN, LOW);
    digitalWrite(ZONE3_VALVE_PIN, LOW);
    for (int i = 0; i < 3; i++) {
        zones[i].valveStatus = false;
    }
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("EMERGENCY STOP ACTIVE");
    Serial.println("All irrigation stopped.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!");
}

// ------------------------------------------------------------
// RESET EMERGENCY STOP
// ------------------------------------------------------------

void resetEmergencyStop() {
    // E-stop must physically be released first
    if (digitalRead(E_STOP_PIN) == LOW) {
        // Reset button pressed
        if (digitalRead(RESET_BUTTON_PIN) == LOW) {
            emergencyStopActive = false;
            irrigationState = IDLE;
            Serial.println("Emergency stop reset.");
        }
    }
}


// ============================================================
// 16. SYSTEM STATUS
// ============================================================

void displaySystemStatus() {
    Serial.println();
    Serial.println("========== AGRIGUARD ==========");
    Serial.print("System state: ");
    switch (irrigationState) {
        case IDLE: Serial.println("IDLE"); break;
        case WATERING: Serial.println("WATERING"); break;
        case SETTLING: Serial.println("SETTLING"); break;
        case CHECKING: Serial.println("CHECKING"); break;
        case MAX_CYCLES_REACHED: Serial.println("MAX CYCLES"); break;
        case EMERGENCY_STOP: Serial.println("EMERGENCY STOP"); break;
    }
    for (int i = 0; i < 3; i++) {
        Serial.print("Zone ");
        Serial.print(i + 1);
        Serial.print(" Soil: ");
        Serial.print(zones[i].currentSoilMoisture);
        Serial.println("%");
    }
    Serial.print("Tank: ");
    Serial.print(waterTank.waterPercentage);
    Serial.println("%");
    Serial.print("Flow: ");
    Serial.print(currentFlowRate);
    Serial.println(" L/min");
    Serial.print("Required water: ");
    Serial.print(requiredWater);
    Serial.println(" L");
    Serial.print("Predicted demand: ");
    Serial.print(waterBudget.predictedDemand);
    Serial.println(" L");
    Serial.println("================================");
}


// ============================================================
// 17. SYSTEM SETUP// ============================================================

void setup() {
    Serial.begin(115200);

    // --------------------------------------------------------
    // PIN MODES
    // --------------------------------------------------------
    
    pinMode(SOIL_SENSOR_1, INPUT);
    pinMode(SOIL_SENSOR_2, INPUT);
    pinMode(SOIL_SENSOR_3, INPUT);
    pinMode(TANK_TRIGGER_PIN, OUTPUT);
    pinMode(TANK_ECHO_PIN, INPUT);
    pinMode(ZONE1_VALVE_PIN, OUTPUT);
    pinMode(ZONE2_VALVE_PIN, OUTPUT);
    pinMode(ZONE3_VALVE_PIN, OUTPUT);
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    pinMode(E_STOP_PIN, INPUT_PULLUP);
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

    // --------------------------------------------------------
    // SAFETY
    // --------------------------------------------------------

    digitalWrite(ZONE1_VALVE_PIN, LOW);
    digitalWrite(ZONE2_VALVE_PIN, LOW);
    digitalWrite(ZONE3_VALVE_PIN, LOW);

    // --------------------------------------------------------
    // FLOW SENSOR INTERRUPT
    // --------------------------------------------------------

    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), countFlowPulse, RISING);

    // --------------------------------------------------------
    // ZONE CONFIGURATION
    // --------------------------------------------------------

    zones[0].zoneID = 1;
    zones[0].soilType = CLAY;
    zones[0].dryCalibration = 3000;
    zones[0].wetCalibration = 1200;
    zones[0].sampleCount = 0;
    zones[0].currentSoilMoisture = 0;
    zones[0].avgSoilMoisture = 0;
    zones[0].valveStatus = false;
    zones[0].sensorPin = SOIL_SENSOR_1;
    zones[0].valvePin = ZONE1_VALVE_PIN;

    zones[1].zoneID = 2;
    zones[1].soilType = SANDY;
    zones[1].dryCalibration = 2800;
    zones[1].wetCalibration = 1000;
    zones[1].sampleCount = 0;
    zones[1].currentSoilMoisture = 0;
    zones[1].avgSoilMoisture = 0;
    zones[1].valveStatus = false;
    zones[1].sensorPin = SOIL_SENSOR_2;
    zones[1].valvePin = ZONE2_VALVE_PIN;

    zones[2].zoneID = 3;
    zones[2].soilType = LOAMY;
    zones[2].dryCalibration = 2900;
    zones[2].wetCalibration = 1100;
    zones[2].sampleCount = 0;
    zones[2].currentSoilMoisture = 0;
    zones[2].avgSoilMoisture = 0;
    zones[2].valveStatus = false;
    zones[2].sensorPin = SOIL_SENSOR_3;
    zones[2].valvePin = ZONE3_VALVE_PIN;

    // --------------------------------------------------------
    // TANK CONFIGURATION
    // --------------------------------------------------------

    // JJ2 = JoJo 5250 L
    waterTank.model = JJ2;

    // --------------------------------------------------------
    // IRRIGATION PROFILE
    // --------------------------------------------------------

    irrigationProfile.zoneID = 1;
    irrigationProfile.irrigationCycles = 0;
    irrigationProfile.averageLitresPerPercentage = 0;
    irrigationProfile.averageDryingRate = 0;
    irrigationProfile.averageWaterPerCycle = 0;
    irrigationProfile.averageCycleDuration = 0;

    // --------------------------------------------------------
    // INITIALIZE TIMERS
    // --------------------------------------------------------

    soilTimer = millis() - SOIL_RECORD_INTERVAL;
    tankTimer = millis() - TANK_RECORD_INTERVAL;
    weatherTimer = millis() - WEATHER_INTERVAL;

    // --------------------------------------------------------
    // WIFI
    // --------------------------------------------------------

    connectWiFi();

    // --------------------------------------------------------
    // INITIAL WEATHER REQUEST
    // --------------------------------------------------------

    getWeatherData();

    Serial.println();
    Serial.println("================================");
    Serial.println("       AGRIGUARD ONLINE");
    Serial.println("================================");
}


// ============================================================
// 18. MAIN LOOP
// ============================================================

void loop() {
    // ========================================================
    // HIGHEST PRIORITY:
    // EMERGENCY STOP
    // ========================================================

    emergencyStop();
    if (emergencyStopActive) {
        // Do not allow normal irrigation logic to execute
        return;
    }
    if (irrigationState == EMERGENCY_STOP) {
        handleEmergencyStop();
        resetEmergencyStop();
        return;
    }

    // ========================================================
    // SENSOR SYSTEMS
    // ========================================================

    for (int i = 0; i < 3; i++) {
        recordSoilData(zones[i]);
    }
    updateTank();

    // ========================================================
    // WEATHER
    // ========================================================

    if (millis() - weatherTimer >= WEATHER_INTERVAL) {
        weatherTimer = millis();
        connectWiFi();
        getWeatherData();
    }

    // ========================================================
    // UPDATE SOIL AVERAGE
    // ========================================================

    for (int i = 0; i < 3; i++) {
        if (zones[i].sampleCount > 0) {
            zones[i].avgSoilMoisture = getAverageSoilMoisture(zones[i]);
        }
    }

    // ========================================================
    // IRRIGATION STATE MACHINE - Process each zone
    // ========================================================

    for (int i = 0; i < 3; i++) {
        switch (irrigationState) {
            case IDLE:
                if (shouldStartIrrigation(zones[i])) {
                    startIrrigation(zones[i]);
                }
                break;
            case WATERING:
                updateWatering(zones[i]);
                break;
            case SETTLING:
                updateSettling(zones[i]);
                break;
            case CHECKING:
                irrigationState = IDLE;
                break;
            case MAX_CYCLES_REACHED:
                digitalWrite(zones[i].valvePin, LOW);
                zones[i].valveStatus = false;
                Serial.println("Maximum irrigation cycles reached.");
                irrigationState = IDLE;
                break;
            case EMERGENCY_STOP:
                handleEmergencyStop();
                break;
        }
    }

    // ========================================================
    // WATER BUDGET
    // ========================================================

    updateWaterBudget();
    if (irrigationState == IDLE) {
        waterBudget.predictedDemand = predictWaterDemand(zones[0]);
    }

    // ========================================================
    // STATUS
    // ========================================================

    static unsigned long statusTimer = 0;
    if (millis() - statusTimer >= 30000UL) {
        statusTimer = millis();
        displaySystemStatus();
    }
}