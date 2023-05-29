#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <WiFiClientSecureBearSSL.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

//Wifi config
const char *ssid = "Cheeloo";
const char *password = "820da49c36320";
WiFiClient client;

//Server config
String serverName = "https://weather-server.fly.dev/api/measure/new-measure";

//BME280
Adafruit_BME280 bme;

//NTP server
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

unsigned long delayMinutes = 30;
unsigned long delayTime =  delayMinutes*60*1000;
//unsigned long delayTime = 3000;
bool status;

struct BME280_read{
  float temperature;
  float humidity;
  float pressure;
};

void connectToWiFi() {
//Connect to WiFi Network
   Serial.print("Connecting to WiFi");
   Serial.println("...");
   WiFi.begin(ssid, password);
   int retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < 15)) {
    retries++;
    delay(500);
    Serial.print(".");
  }
  if (retries > 14) {
      Serial.println(F("WiFi connection FAILED"));
  }
  if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("WiFi connected!"));
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      timeClient.begin();
      //digitalWrite(LED_BUILTIN, LOW); //when connected turn on built in led
  }
    Serial.println(F("Setup ready"));
}

void printLocalTime()
{
  time_t rawtime;
  struct tm * timeinfo;
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  Serial.println(asctime(timeinfo));
  delay(1000);
}

void setup() {
  //pinMode(LED1, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); //turn off built in led, by default its on
  Serial.begin(9600);
  connectToWiFi();

  Serial.println(F("BME280 test"));

  status = bme.begin(0x76); 
  if(!status){
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }

}

void loop() {
  if(status){
    printValues();
    saveMeasure();
    delay(delayTime);
    //testSSLConnection();
  
  }
}

void testSSLConnection(){
  if (WiFi.status() == WL_CONNECTED) {
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      client->setInsecure();
      HTTPClient https;
      if (https.begin(*client, "https://www.howsmyssl.com/a/check")) {  // HTTPS
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();
      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }
  }
}

String getDateFromNTP(){
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);
  String ISODateTime;

  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  String currentMonthStr;
  if(currentMonth>=1 && currentMonth<10){ // we need month in format 08,09,10, there must be zero
    currentMonthStr="0"+String(currentMonth);
  }
  else{
    currentMonthStr=String(currentMonth);
  }
  int currentYear = ptm->tm_year+1900;
  ISODateTime=String(currentYear) + "-" + currentMonthStr + "-" + String(monthDay)
           +"T"+String(timeClient.getFormattedTime())+"Z";
  return ISODateTime;
}

void saveMeasure(){
  BME280_read bme280_read;
  bme280_read.temperature=bme.readTemperature();
  bme280_read.humidity=bme.readHumidity();
  bme280_read.pressure=bme.readPressure();
  uint32 stationId=system_get_chip_id();

  if (WiFi.status() == WL_CONNECTED) {
      std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
      client->setInsecure();
      HTTPClient https;
      if (https.begin(*client, serverName)) {  // HTTPS
        //define json to send
        DynamicJsonDocument doc(1024);

        String date = getDateFromNTP();

        doc["stationId"] = stationId;
        doc["date"] = date;
        doc["temp"] = round(bme280_read.temperature*100)/100;
        doc["humidity"] = round(bme280_read.humidity*100)/100;
        doc["pressure"] = round(bme280_read.pressure*100)/100;

        String json;
        serializeJson(doc, json);
        Serial.println(json);

        Serial.print("[HTTPS] POST...\n");

        https.addHeader("Content-Type", "application/json");
        int httpCode = https.POST(json);
        if (httpCode > 0) {
          Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            Serial.println(payload);
          }
        } else {
          Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
        https.end();
      } else {
      Serial.printf("[HTTPS] Unable to connect\n");
      }
  }
}
void printValues() {
  Serial.print("Temperature = ");
  Serial.print(bme.readTemperature());
  Serial.println(" *C");
  
  Serial.print("Pressure = ");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  Serial.println();
}
