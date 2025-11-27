#include <WiFi.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Firebase_ESP_Client.h> // v4

/////////////////////////
// CONFIG
/////////////////////////
const char* WIFI_SSID = "NayaTel";       // hardcoded Wi-Fi
const char* WIFI_PASS = "Tr9b5ey1";

#define FIREBASE_HOST "wind-speed-a5223-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "zzB9UhxmW3lt6YXzAgPQQvHfT4d3s55UCLRBpFpG"

const float anemometerDiameter = 0.12; // meters
const int pulsesPerRevolution = 2;
const unsigned long updateInterval = 15000UL;
const int IR_PIN = 21;

/////////////////////////
// GLOBALS
/////////////////////////
volatile unsigned long pulseCount = 0;
unsigned long lastUpdateMillis = 0;

TFT_eSPI tft = TFT_eSPI();
float displaySpeed = 0.0f;
float targetSpeed = 0.0f;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

/////////////////////////
// ISR
/////////////////////////
void IR_ISR() { pulseCount++; }

float calculateSpeedFromPulses(unsigned long pulses, unsigned long intervalMs){
  float revs = (float)pulses / pulsesPerRevolution;
  float dist = revs * 3.1416 * anemometerDiameter;
  float intervalSec = (float)intervalMs / 1000.0f;
  if(intervalSec <=0.0f) return 0.0f;
  return dist / intervalSec;
}

/////////////////////////
// DISPLAY
/////////////////////////
void drawStartupName(){
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, tft.height()/2 - 10);
  tft.print("Muhammad Sultan");
  tft.setTextSize(1);
  tft.setCursor(10, tft.height()/2 + 16);
  tft.print("Counting pulses...");
}

void drawSpeedStatic(float speed){
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2); tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE); tft.setCursor(8,8);
  tft.print("AERON - Anemometer");
  tft.setCursor(8,22); tft.print("WiFi: Connected");

  int bigX=8, bigY=36;
  tft.setTextFont(7); tft.setTextSize(2);
  tft.setTextColor(TFT_DARKGREY); tft.setCursor(bigX+3,bigY+3); tft.print((int)speed);
  tft.setTextColor(TFT_WHITE); tft.setCursor(bigX,bigY); char buf[12]; dtostrf(speed,0,1,buf); tft.print(buf);
  tft.setTextFont(4); tft.setTextSize(1); tft.setCursor(bigX+140,bigY+36); tft.print("m/s");
}

/////////////////////////
// FIREBASE
/////////////////////////
void firebaseInit(){
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config,&auth);
  Firebase.reconnectWiFi(true);
}

void uploadToFirebase(float speed){
  String path="/anemometer/latest";
  FirebaseJson json;
  json.set("windSpeed", speed);
  json.set("timestamp", millis()/1000);
  if(!Firebase.RTDB.setJSON(&fbdo,path,&json)) Serial.println("Firebase upload failed");
  else Serial.println("Uploaded: "+String(speed));
}

/////////////////////////
// SETUP
/////////////////////////
void setup(){
  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(IR_PIN), IR_ISR, RISING);
  tft.init();
  tft.setRotation(1);
  drawStartupName();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting Wi-Fi");
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
  firebaseInit();
  lastUpdateMillis = millis();
}

/////////////////////////
// LOOP
/////////////////////////
void loop(){
  unsigned long now = millis();
  if(now - lastUpdateMillis >= updateInterval){
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();
    lastUpdateMillis = now;

    float measuredSpeed = calculateSpeedFromPulses(count, updateInterval);
    targetSpeed = measuredSpeed;

    uploadToFirebase(measuredSpeed);
    drawSpeedStatic(displaySpeed);
  }

  float diff = targetSpeed - displaySpeed;
  float step = diff*0.12f;
  if(abs(step)<0.01f) displaySpeed=targetSpeed;
  else displaySpeed+=step;

  delay(60);
}
