#define triggerPin 12
#define echoPin 14

const unsigned long timeStamp = 3600000;   // 1 hour
unsigned long timeCount = 0;

//This represents different water tank models
enum TankModel              
{
    JJ1,
    JJ2,
    JJ3,
    JJ4,
    JJ5,
    JJ6
};

// Tank information that never changes
struct TankProfile
{
    String manufacturer;
    int volume;        // Litres
    float height;      // cm
    float diameter;    // cm
};

// Current tank readings
struct WaterTank
{
    TankModel model;
    float echoTime;
    float distance;
    float waterHeight;
    float waterPercentage;
};

//This is reference table to compare reaading with data in the table
TankProfile tankProfiles[] =
{
    {"JoJo", 2000, 145.0, 142.0},
    {"JoJo", 5250, 225.5, 182.0},
    {"JoJo", 5500, 224.0, 190.0},
    {"JoJo",10000, 315.0, 220.0},
    {"JoJo",15000, 326.0, 260.0},
    {"JoJo",20000, 427.0, 260.0}
};

WaterTank waterZn;

void setup()
{
    Serial.begin(115200);

    pinMode(triggerPin, OUTPUT);
    pinMode(echoPin, INPUT);

    // This will depend on what the user selects
    waterZn.model = JJ2;
}

// Read ultrasonic sensor
void recordTankData()
{
    digitalWrite(triggerPin, LOW);
    delayMicroseconds(2);

    digitalWrite(triggerPin, HIGH);
    delayMicroseconds(10);

    digitalWrite(triggerPin, LOW);

    waterZn.echoTime = pulseIn(echoPin, HIGH, 30000);
}

// Convert echo time into distance
void convertDistance(){
    // Distance in cm
    waterZn.distance = (waterZn.echoTime * 0.0343) / 2.0;       //The 0.0343 is the speed of sound
}

// Calculate water level
void calculateWaterLevel(){

    TankProfile currentTank = tankProfiles[waterZn.model];

    waterZn.waterHeight = currentTank.height - waterZn.distance;

    if(waterZn.waterHeight < 0)
        waterZn.waterHeight = 0;
    else if(waterZn.waterHeight > 100)
        waterZn.waterHeight = 100;
    
    waterZn.waterPercentage =
        (waterZn.waterHeight / currentTank.height) * 100.0;
}

void displayResults()
{
    TankProfile currentTank = tankProfiles[waterZn.model];

    Serial.print("Tank: ");
    Serial.println(currentTank.volume);

    Serial.print("Distance: ");
    Serial.print(waterZn.distance);
    Serial.println(" cm");

    Serial.print("Water Height: ");
    Serial.print(waterZn.waterHeight);
    Serial.println(" cm");

    Serial.print("Water Level: ");
    Serial.print(waterZn.waterPercentage);
    Serial.println("%");
}

void loop(){
    if(millis() - timeCount >= timeStamp)
    {
        timeCount = millis();

        recordTankData();
        convertDistance();
        calculateWaterLevel();
        displayResults();
    }
}