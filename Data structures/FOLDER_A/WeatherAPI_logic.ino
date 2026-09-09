#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Weather variables ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const int DAILY_DATA = 48;
const unsigned long WEATHER_INTERVAL = 60UL * 60UL * 1000UL;  // 1 hour
const float RAIN_THRESHOLD = 60.0;
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

//Coordinates of an area
const float LAT = -29.35080374459709;
const float LON = 27.601807742259524;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Weather structure ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct weather_API{
  String time_Stamp;
  float API_temp;
  float API_humidity;
  float rain_prob;
};



//URL for getting weather data
String API_URL = "https://api.open-meteo.com/v1/forecast?
                  latitude="+LAT+"&longitude="+LON+"&hourly=temperature_2m,
                  relative_humidity_2m,precipitation_probability,
                  precipitation&forecast_days=2&timezone=auto";


weather_API data[DAILY_DATA];

void connect_WiFi(){

  WiFi.begin(ssid, password);
  unsigned long startAttempt = millis();

  while(WiFi.status() != WL_CONNECTED && millis() - startAttempt < 5000){ //Time difference = 5s
        Serial.println("Reconnecting...");
        delay(500);
        Serial.print(".");
        
    }

  if(WiFi.status() == WL_CONNECTED){
   Serial.println("WiFi connected.");
  }

}

void getWeatherData(){

 //This condition checks if the system has connected to the internet
 if(WiFi.status() == WL_CONNECTED){       
   HTTPClient httpObj;          //This object will enable the system to make HTTP requests
  
   //HTTP CONNECTION TIMEOUT
    httpObj.setConnectTimeout(5000);  // 5 sec to establish connection
    httpObj.setTimeout(10000);         // 10 sec waiting for data
   String requestURL = API_URL;

   httpObj.begin(requestURL);       //Sends an HTTP request with all data included e.g (LAT, LON, API_KEY).

    int httpCode = httpObj.GET();        //Creates a connection to send the HTTP request
    
  
   if(httpCode == HTTP_CODE_OK){
    String Json_Data = httpObj.getString();      //Read data 
    Serial.println(Json_Data);

    //Retrieves weather data as json
    DynamicJsonDocument doc(8192);            //Memory allocated to store Json data
    DeserializationError error = deserializeJson(doc, Json_Data);

    if(error){
    Serial.print("JSON error: ");
    Serial.println(error.c_str());
    httpObj.end();
    return;
    }

    JsonObject jsonOb = doc.as<JsonObject>();

    //Display data
   for (int a = 0; a < DAILY_DATA; a++) {
    data[a].time_Stamp = jsonOb["hourly"]["time"][a].as<String>();

    data[a].API_temp = jsonOb["hourly"]["temperature_2m"][a].as<float>();

    data[a].API_humidity = jsonOb["hourly"]["relative_humidity_2m"][a].as<float>();

    data[a].rain_prob = jsonOb["hourly"]["precipitation_probability"][a].as<float>();
   }
    
   } else{
    Serial.println("Error: Cannot retrieve data!");
    Serial.println(httpCode);
   }

   httpObj.end();
  }
}

void setup(){
  Serial.begin(115200);
   connect_WiFi();
}

