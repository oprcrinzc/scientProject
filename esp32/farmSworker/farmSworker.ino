#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>
#include <Wire.h>
#include <DHT.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include <sstream>

#define USE_SERIAL Serial

WiFiMulti wifiMulti;

DHT dht(19, DHT22);

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

String worker_name;
String worker_id;
int state = 0;
int is_registered = 0;

String dataserver = "http://192.168.21.185:8888";

int waterLevelPin = 35;
float waterLevel;
float waterLevelTarget;
float waterLevelToFill;
float h;
float t;
float f;

int pump_state;
int is_pump_working;


JsonDocument data;

std::vector<int> btn({34});
std::vector<int> btn_last_state(btn.size());
std::vector<int> btn_current_state(btn.size());

int playMode = 0;
int lastPlayMode = 0;
int munit = 0; 

void see_gatekeeper(){
  if ((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;

    // USE_SERIAL.print("[HTTP] begin...\n");
    if (is_registered==0){
      http.begin(dataserver+"/gatekeeper/"+worker_name);

      // USE_SERIAL.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      // USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        if (payload=="go to register") state = 1;
        else {
          worker_id = payload;
          is_registered =1 ;
          state=2;
        }
        // USE_SERIAL.println(payload);
      }else {
        // USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
    }
  } 
  http.end();
  }
}


void goto_register(){
  if ((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;

    // USE_SERIAL.print("[HTTP] begin...\n");
    if (is_registered==0){
      http.begin(dataserver+"/register/workers/"+worker_name);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      // USE_SERIAL.print("[HTTP] POST...\n");
      // start connection and send HTTP header
      int httpCode = http.POST("");

      // httpCode will be negative on error
      if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      // USE_SERIAL.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == 201) {
        String payload = http.getString();
        // if (payload=="go to register") state = 1;
        state = 0;
        is_registered = 1;
        // USE_SERIAL.println(payload);
      }else {
        // USE_SERIAL.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
    }
  } 
  http.end();
  }
}
void update_data(){
  if ((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;

    // USE_SERIAL.print("[HTTP] begin...\n");
    if (is_registered==1){
      String url_ =dataserver+"/update/workers/"+worker_name;
      http.begin(url_);
      String boundary = "----Eeeboundary";
      http.addHeader("Content-Type", "multipart/form-data; boundary="+boundary);
      String body = "";
      body += "--"+boundary + "\r\n";
      body += "Content-Disposition: form-data; name=\"id\"\r\n\r\n";
      body += worker_id.substring(1, 25)+"\r\n";

      body += "--"+boundary + "\r\n";
      body += "Content-Disposition: form-data; name=\"humidity\"\r\n\r\n";
      body += String(h, 2)+"\r\n";

      body += "--"+boundary + "\r\n";
      body += "Content-Disposition: form-data; name=\"temperature\"\r\n\r\n";
      body += String(t, 2)+"\r\n";

      body += "--"+boundary + "\r\n";
      body += "Content-Disposition: form-data; name=\"water_level\"\r\n\r\n";
      body += String(waterLevel, 1)+"\r\n";
      body += "--"+boundary + "\r\n";
      int httpCode = http.PUT(body);

      // httpCode will be negative on error
      if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      // USE_SERIAL.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == 201) {
        String payload = http.getString();
        // if (payload=="go to register") state = 1;
        // state = 2;
        // USE_SERIAL.println("updated");
      }else {
        // USE_SERIAL.println(url_);
        // USE_SERIAL.println(body);
        // USE_SERIAL.printf("[HTTP] POST failed, error: %s\n",http.errorToString(httpCode).c_str());
      }
    }
  } 
  http.end();
  }
}
void get_data(){
  if ((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;

    // USE_SERIAL.print("[HTTP] begin...\n");
    if (is_registered==1){
      http.begin(dataserver+"/fetch/worker/"+worker_name);

      // USE_SERIAL.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      // USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DeserializationError error = deserializeJson(data, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        // Serial.println(payload);
        if(data.containsKey("water_level_target")){
          // Serial.print("water level target: ");
          waterLevelTarget = data["water_level_target"].as<float>();
          waterLevelToFill = data["water_level_to_fill"].as<float>();
          // Serial.println(wlt);
          // Serial.print("\n");
        }
      }else {
        // USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
    }
  } 
  http.end();
  }
}


void pumpLogic(){
  if(waterLevelToFill > waterLevel) {
    is_pump_working = 1;
    pump_state = 1;
  }
  if(waterLevel >= waterLevelTarget){
    is_pump_working = 0;
  }
}



void updateBtnState(int mode=0){
  if(mode==0){
    for(int i=0;i< btn.size();i++){
      btn_current_state[i] = digitalRead(btn[i]);
    }
  } else if(mode == 1){
    btn_last_state = btn_current_state;
  }
}


void toggleCMMM(){
  munit += 1;
  if(munit>5) munit = 0;
}


void setup() {

  USE_SERIAL.begin(115200);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  // wifiMulti.addAP("natasa_2.4G", "S141036p");
  wifiMulti.addAP("🌸", "wpaoknao");


  worker_name = "esp32_New";

  pinMode(18, OUTPUT);
  pinMode(35, INPUT);
  digitalWrite(18, HIGH);
  dht.begin();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);

  for(int pin : btn) pinMode(pin, INPUT_PULLUP);
  updateBtnState(0);
  updateBtnState(1);
  
  is_pump_working = 0;
  pump_state = 0;
}

unsigned long previousMillis = 0; // Store last time
const long interval = 500;      // Interval in milliseconds



void loop() {
  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true);
  float waterLevelRaw = analogRead(waterLevelPin);
  waterLevel = (waterLevelRaw)/4095.0*100;
  Serial.print(waterLevelRaw-2000.0);
  Serial.print(" ");
  Serial.print(waterLevel);
  Serial.print("\n");

  unsigned long currentMillis = millis();
  
  if (state==0) see_gatekeeper();
  if (state==1) goto_register();
  

  if (state==2 && currentMillis - previousMillis >= interval){
    previousMillis = currentMillis;
    update_data();
  }
  if (state==2) get_data();

  updateBtnState(0);
  int btn_num = 0;
  for(int state : btn_current_state){
    if(btn_num==0 && state==1 && btn_last_state.at(btn_num) == 0) {
      toggleCMMM();
      Serial.println(state);
      }
    btn_num+=1;
  }
  
  float show;
  std::string units;
  if(munit==0){units="C";show=t;}
  else if(munit==1){units="F";show=f;}
  else if(munit==2){units="%";show=h;}
  else if(munit==3){units="WL";show=waterLevel;}
  else if(munit==4){units="WT";show=waterLevelTarget;}
  else if(munit==5){units="WF";show=waterLevelToFill;}
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(show);
  display.setCursor(100,15);
  display.setTextSize(2);
  display.print(units.c_str());
  display.display();
  updateBtnState(1);  
}
