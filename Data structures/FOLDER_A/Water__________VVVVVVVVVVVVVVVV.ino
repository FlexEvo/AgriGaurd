#define waterValvePin 26
// Flow sensor
#define FLOW_SENSOR_PIN 18

enum IrrigationState {
  IDLE,
  WATERING,
  SETTLING,
  CHECKING,
  MAX_CYCLES_REACHED
};

IrrigationState irrigationState = IDLE;

// ---------------- Flow sensor ----------------
const float PULSES_PER_LITRE = 450.0;
const unsigned long FLOW_RATE_WINDOW = 5000UL;


// ---------------- IRRIGATION VARIABLES ----------------

//unsigned long stateStartTime = 0;

unsigned long irrigationDuration = 0;
//unsigned long settlingTime = 0;

int irrigationCycles = 0;
const int maxCycles = 3;

//~~~~~~~~~~~~~~Stuff here~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


void getNewSoilReadings(Zone& zone){

 float newReadings = analogRead(sensorOutPut);
 float placeholder = 0;

 placeholder = (zone.dryCalibration - newReadings) / (zone.dryCalibration - zone.wetCalibration) * 100;      //Convert each data into a percentage

 if(placeholder > 100){
 placeholder = 100;
  }
 else if(placeholder < 0){
  placeholder = 0;
    }

  zone.newSoilReading = placeholder;
}

float getStopDryingRate(Zone& zone){

  float dryRate = 0;
  float moistureChange = zone.soilMoistureData[sampleSize - 1]- zone.soilMoistureData[0];


  float timeChange = zone.timeStampData[sampleSize - 1] - zone.timeStampData[0];

  // Prevent division by zero
  if (timeChange <= 0){
    return dryRate;
   }
   else{
    dryRate = abs(moistureChange / timeChange);
   return dryRate;
  }
}

float getStopThreshold(Zone& zone){

  float dryRate = getStopDryingRate(zone);
  double soilStopThreshold = 0;
  double loamyS1_threshold = 56;
  double clayS1_threshold = 70;
  double SandyS1_threshold = 48;
  float tempPrec = data.API_temp;              //Gets temperature
  float humidPerc = data.API_humidity;              //Gets temperature
  const double TEMP_COEFFICIENT = 3.97;              //Temperature coeficients to convert percentage into usable values
  const double HUMID_COEFFICIENT = 5.97;             //Humidity coeficients to convert percentage into usable values
  const double DRYING_RATE_COEFFICIENT = 7.96;       //Drying rate coeficients to convert percentage into usable values

  //Data effect stuff
 /*
  -> For temperature:
    We use 20 as a threshold adjuster and the (-1, +1) are for making sure extreme temperatures don't affect the calibration.

  -> For humidity:  
    We use 60 as a threshold adjuster and the (-1, +1) are for making sure extreme humidity don't affect the calibration.

  -> For drying rate:
    We use 60 as a threshold adjuster and the (-1, +1) are for making sure extreme drying rate don't affect the calibration.
 */ 
  float tempEffect = constrain((tempPrec - 20.0) / 15.0, -1.0, 1.0);

  float humidityEffect = constrain((60.0 - humidPerc) / 35.0, -1.0, 1.0);

  float dryingEffect = constrain((dryRate - 0.5) / 1.5, -1.0, 1.0);


 //This determines the threshold for every hour
  if(arrIndx < DAILY_DATA){
    if(zone.soilTyp == CLAY){
    soilStopThreshold = abs(clayB1_thresholds + (tempEffect * TEMP_COEFFICIENT) + (dryRate * DRYING_RATE_COEFFICIENT) - (humidPerc * HUMID_COEFFICIENT));
  } 
  else if(zone.soilTyp == SANDY){
    soilStopThreshold = abs(SandyB1_thresholds + (tempEffect * TEMP_COEFFICIENT) + (dryRate * DRYING_RATE_COEFFICIENT) - (humidPerc * HUMID_COEFFICIENT));
  }
  else if(zone.soilTyp == LOAMY){
    soilStopThreshold = abs(loamyB1_thresholds + (tempEffect * TEMP_COEFFICIENT) + (dryRate * DRYING_RATE_COEFFICIENT) - (humidPerc * HUMID_COEFFICIENT));
    }
  } 

  if (soilStopThreshold > 100)
    soilStopThreshold = 100;

  else if (soilStopThreshold < 0)
    soilStopThreshold = 0;

  return soilStopThreshold;
}

bool stopIrrigation(Zone& zone){

  float stopThreshold = getStopThreshold(zone);
  float waterLvl = waterZn.waterPercentage;
  float soilMoisture = zone.newSoilReading;

  if(soilMoisture >= stopThreshold || waterLvl < 30){
  zone.valveStatus = false;            //Turns off watering valves
  }

  return zone.valveStatus;
}

//Come back to find better approach for duration
float irrigationDuration(Zone& zone){

  float moistureDeficit = 0;
  float stopThreshold = getStopThreshold(zone);
  float currentMoisture = zone.newSoilReading;
  float dryRate = getStopDryingRate();

  moistureDeficit = (stopThreshold - currentMoisture) * dryRate;

  return moistureDeficit;
}

// ---------------- START IRRIGATION DECISION ----------------

bool shouldStartIrrigation() {

  float startThreshold = getThreshold();

  float soilMoisture = zone1.newSoilReading;

  float waterLevel = waterZn.waterPercentage;
  bool test = false;


  // Checks if water isn't critical and moisture is low

  if (waterLevel > 30 && soilMoisture < startThreshold) {

    test = true;
  }

  return test;
}

// ---------------- SOIL-SPECIFIC PARAMETERS ----------------

void setIrrigationParameters() {

  switch (zone1.soilTyp) {

    case CLAY:

      irrigationDuration = 5UL * 60UL * 1000UL;

      // Clay needs more time for water to distribute
      settlingTime = 15UL * 60UL * 1000UL;

      break;


    case LOAMY:

      irrigationDuration = 7UL * 60UL * 1000UL;

      settlingTime = 10UL * 60UL * 1000UL;

      break;


    case SANDY:

      irrigationDuration = 5UL * 60UL * 1000UL;

      settlingTime = 5UL * 60UL * 1000UL;

      break;


    default:

      irrigationDuration = 5UL * 60UL * 1000UL;
      settlingTime = 10UL * 60UL * 1000UL;

      break;
  }
}

// ---------------- IRRIGATION STATE MACHINE ----------------

void updateIrrigation() {

  switch (irrigationState) {
    // ==================================================
    // IDLE
    // ==================================================

    case IDLE:
     {
       // Make sure valve is OFF
      digitalWrite(waterValvePin, LOW);

      // Check tank first
      if (waterZn.waterPercentage <= 30) {

        Serial.println("Tank level too low.");
        break;
      }


      // Check whether irrigation is needed
      if (shouldStartIrrigation()) {

        Serial.println("Irrigation required.");

        // Determine soil-specific irrigation settings
        setIrrigationParameters();

        irrigationCycles = 0;

        // Start irrigation
        digitalWrite(waterValvePin, HIGH);

        Serial.println("Valve ON.");

        stateStartTime = millis();

        irrigationState = WATERING;
      }

      break;

     }

   
   
    // ==================================================
    // WATERING
    // ==================================================

    case WATERING:
    {
      
      // Safety check for tank
      if (waterZn.waterPercentage <= 30) {

        digitalWrite(waterValvePin, LOW);

        Serial.println("Tank too low. Valve OFF.");

        irrigationState = IDLE;

        break;
      }


      // Has watering duration finished?
      if (millis() - stateStartTime >= irrigationDuration) {

        digitalWrite(waterValvePin, LOW);

        irrigationCycles++;

        Serial.println("Watering cycle complete.");
        Serial.print("Cycle: ");
        Serial.println(irrigationCycles);


        // Start settling period
        stateStartTime = millis();

        irrigationState = SETTLING;
      }

      break;

    }

    // ==================================================
    // SETTLING
    // ==================================================
    case SETTLING:
    {
            // Allow water to distribute through soil
      if (millis() - stateStartTime >= settlingTime) {

        Serial.println("Settling complete.");

        irrigationState = CHECKING;
      }

      break;

    }

    // ==================================================
    // CHECKING
    // ==================================================

    case CHECKING:
    {
            Serial.println("Taking new soil measurement...");

      // IMPORTANT:
      // This is a completely new physical sensor reading.
      getNewSoilReadings();


      Serial.print("Stop threshold: ");
      Serial.print(getStopThreshold());
      Serial.println("%");


      // Check tank again
      if (waterZn.waterPercentage <= 30) {

        Serial.println("Tank too low. Irrigation stopped.");

        irrigationState = IDLE;

        break;
      }


      // Has soil reached the required moisture?
      if (zone1.newSoilReading >= getStopThreshold()) {

        Serial.println("Soil moisture sufficient.");
        Serial.println("Irrigation stopped.");

        irrigationState = IDLE;
      }


      // Soil is STILL too dry
      else {

        Serial.println("Soil still below stop threshold.");

        // Maximum cycle protection
        if (irrigationCycles >= maxCycles) {

          Serial.println("Maximum irrigation cycles reached.");

          irrigationState = MAX_CYCLES_REACHED;
        }

        else {

          Serial.println("Starting another irrigation cycle.");

          // Start another watering cycle
          digitalWrite(waterValvePin, HIGH);

          stateStartTime = millis();

          irrigationState = WATERING;
        }
      }

      break;
    }


    // ==================================================
    // MAXIMUM CYCLES REACHED
    // ==================================================

    case MAX_CYCLES_REACHED:
      // ALWAYS turn valve OFF
      digitalWrite(waterValvePin, LOW);

      Serial.println(
        "Irrigation stopped: maximum cycles reached."
      );

      // Don't immediately start another cycle
      irrigationState = IDLE;

      break;
  }
}
