#include <WiFi.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Firebase_ESP_Client.h>
#include <time.h>

/////////////////////////
// CONFIG
/////////////////////////
const char* WIFI_SSID = "NayaTel";
const char* WIFI_PASS = "Tr9b5ey1";

#define FIREBASE_HOST "wind-speed-a5223-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "zzB9UhxmW3lt6YXzAgPQQvHfT4d3s55UCLRBpFpG"

const float anemometerDiameter = 0.12; // meters
const int pulsesPerRevolution = 2;
const unsigned long updateInterval = 15000UL; // 15 seconds
const int IR_PIN = 21;

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 18000; // UTC+5 for Pakistan
const int daylightOffset_sec = 0;

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
void IR_ISR() { 
  pulseCount++; 
}

float calculateSpeedFromPulses(unsigned long pulses, unsigned long intervalMs){
  float revs = (float)pulses / pulsesPerRevolution;
  float dist = revs * 3.1416 * anemometerDiameter;
  float intervalSec = (float)intervalMs / 1000.0f;
  if(intervalSec <= 0.0f) return 0.0f;
  return dist / intervalSec;
}

/////////////////////////
// TIME
/////////////////////////
unsigned long getEpochTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return millis() / 1000; // Fallback
  }
  time(&now);
  return (unsigned long)now;
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
  tft.print("Initializing...");
}

void drawSpeedStatic(float speed){
  tft.fillScreen(TFT_BLACK);
  
  // Header
  tft.setTextFont(2); 
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN); 
  tft.setCursor(8, 8);
  tft.print("AERON - Anemometer");
  
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(8, 22); 
  tft.print("WiFi: Connected");

  // Speed display
  int bigX = 8, bigY = 50;
  tft.setTextFont(7); 
  tft.setTextSize(2);
  
  // Shadow effect
  tft.setTextColor(TFT_DARKGREY); 
  tft.setCursor(bigX + 3, bigY + 3); 
  tft.print((int)speed);
  
  // Main text
  tft.setTextColor(TFT_WHITE); 
  tft.setCursor(bigX, bigY); 
  char buf[12]; 
  dtostrf(speed, 0, 1, buf); 
  tft.print(buf);
  
  // Unit
  tft.setTextFont(4); 
  tft.setTextSize(1); 
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(bigX + 140, bigY + 36); 
  tft.print("m/s");
  
  // Footer
  tft.setTextFont(2);
  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(8, tft.height() - 20);
  tft.print("BS Physics Project");
}

/////////////////////////
// FIREBASE
/////////////////////////
void firebaseInit(){
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  fbdo.setBSSLBufferSize(1024, 1024);
}

void uploadToFirebase(float speed){
  unsigned long timestamp = getEpochTime();
  
  // Update latest reading
  String latestPath = "/anemometer/latest";
  FirebaseJson latestJson;
  latestJson.set("windSpeed", speed);
  latestJson.set("timestamp", timestamp);
  latestJson.set("unit", "m/s");
  
  if(Firebase.RTDB.setJSON(&fbdo, latestPath, &latestJson)) {
    Serial.println("Latest updated: " + String(speed) + " m/s");
  } else {
    Serial.println("Latest update failed: " + fbdo.errorReason());
  }
  
  // Add to history (for charts and statistics)
  String historyPath = "/anemometer/history/" + String(timestamp);
  FirebaseJson historyJson;
  historyJson.set("speed", speed);
  historyJson.set("timestamp", timestamp);
  
  if(Firebase.RTDB.setJSON(&fbdo, historyPath, &historyJson)) {
    Serial.println("History logged");
  } else {
    Serial.println("History log failed: " + fbdo.errorReason());
  }
  
  // Clean old data (keep last 24 hours = 86400 seconds)
  cleanOldData(timestamp);
}

void cleanOldData(unsigned long currentTimestamp) {
  // Only clean every hour to reduce operations
  static unsigned long lastCleanTime = 0;
  if (currentTimestamp - lastCleanTime < 3600) return;
  
  lastCleanTime = currentTimestamp;
  unsigned long cutoffTime = currentTimestamp - 86400; // 24 hours ago
  
  Serial.println("Cleaning old data...");
  
  // Query and delete old entries
  if (Firebase.RTDB.getJSON(&fbdo, "/anemometer/history")) {
    FirebaseJson &json = fbdo.jsonObject();
    size_t len = json.iteratorBegin();
    
    for (size_t i = 0; i < len; i++) {
      FirebaseJson::IteratorValue value = json.valueAt(i);
      unsigned long ts = String(value.key).toInt();
      
      if (ts < cutoffTime) {
        String deletePath = "/anemometer/history/" + String(value.key);
        Firebase.RTDB.deleteNode(&fbdo, deletePath);
        Serial.println("Deleted old entry: " + String(value.key));
      }
    }
    json.iteratorEnd();
  }
}

/////////////////////////
// SETUP
/////////////////////////
void setup(){
  Serial.begin(115200);
  
  pinMode(IR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_PIN), IR_ISR, RISING);
  
  tft.init();
  tft.setRotation(1);
  drawStartupName();

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20){
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.println("IP: " + WiFi.localIP().toString());
    
    // Initialize time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Time synchronized");
    
    // Initialize Firebase
    firebaseInit();
    Serial.println("Firebase initialized");
  } else {
    Serial.println("\nWiFi connection failed!");
  }
  
  lastUpdateMillis = millis();
  
  // Initial display
  drawSpeedStatic(0.0);
}

/////////////////////////
// LOOP
/////////////////////////
void loop(){
  unsigned long now = millis();
  
  // Upload data every interval
  if(now - lastUpdateMillis >= updateInterval){
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();
    
    lastUpdateMillis = now;

    float measuredSpeed = calculateSpeedFromPulses(count, updateInterval);
    targetSpeed = measuredSpeed;

    // Upload to Firebase
    if(WiFi.status() == WL_CONNECTED) {
      uploadToFirebase(measuredSpeed);
    } else {
      Serial.println("WiFi disconnected, attempting reconnect...");
      WiFi.reconnect();
    }
    
    // Update display
    drawSpeedStatic(displaySpeed);
  }

  // Smooth animation for display
  float diff = targetSpeed - displaySpeed;
  float step = diff * 0.12f;
  if(abs(step) < 0.01f) {
    displaySpeed = targetSpeed;
  } else {
    displaySpeed += step;
  }

  delay(60);
}
