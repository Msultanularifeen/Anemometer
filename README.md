--- README.md ---
# ESP32 Anemometer Project

Files included:
- `main.ino` — ESP32 Arduino sketch (reads IR pulses, calculates speed, shows on ILI9486 TFT, uploads to Firebase Realtime DB, and has WiFi AP fallback portal).
- `index.html` — Modern web UI that reads Firebase Realtime Database and shows current speed, average speed, and a live chart (uses Chart.js).
- `style.css` — Simple styling for the web UI.

## Quick setup
1. Install Arduino libraries: TFT_eSPI, HTTPClient, ArduinoJson (optional), WiFi, Preferences.
2. Configure `TFT_eSPI` library for your ILI9486 (edit `User_Setup.h` in TFT_eSPI examples). If you prefer Adafruit drivers, adapt code accordingly.
3. Edit constants at top of `main.ino` (wheel diameter, pulsesPerRevolution, FIREBASE_HOST, FIREBASE_AUTH if needed).
4. Upload `index.html` + `style.css` to any static host (GitHub Pages, Firebase Hosting, or your own webserver). The web UI reads directly from your Firebase Realtime DB via REST.

--- main.ino ---
/*
  ESP32 Anemometer
  - Counts pulses from an IR sensor using interrupt
  - Calculates current speed (m/s and km/h) and average over a sliding window
  - Shows data on ILI9486 3.5" TFT using TFT_eSPI
  - Uploads readings to Firebase Realtime Database via REST API
  - If WiFi not available, starts SoftAP + simple portal to enter SSID/password

  Wiring (example):
   - IR sensor digital out -> GPIO 13 (IR_PIN)
   - GND -> GND, VCC -> 3.3V or 5V per your sensor
   - TFT: configure via TFT_eSPI User_Setup.h for ILI9486 (pins depend on shield/module)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>

// ======= CONFIG =======
#define IR_PIN 13                // interrupt pin for IR pulse
#define PULSES_PER_REV 1         // number of IR pulses per full rotation (set 1 if one pulse per rev)
const float WHEEL_DIAMETER_M = 0.10; // meters (set your anemometer cup wheel diameter)
const char* FIREBASE_HOST = "wind-speed-a5223-default-rtdb.firebaseio.com"; // user provided
const char* FIREBASE_AUTH = "zzB9UhxmW3lt6YXzAgPQQvHfT4d3s55UCLRBpFpG";      // user provided

// WiFi and portal
Preferences prefs;
WebServer server(80);
TFT_eSPI tft = TFT_eSPI();

// ======= GLOBALS =======
volatile unsigned long pulseCount = 0; // incremented in ISR
unsigned long lastSampleMillis = 0;
const unsigned long SAMPLE_INTERVAL_MS = 2000; // compute every 2s

// For averaging over N samples
const int AVERAGE_WINDOW = 10;
float speedWindow[AVERAGE_WINDOW];
int speedWindowIdx = 0;
int speedWindowCount = 0;

// WiFi credentials saved in Preferences keys: ssid / pass

// ======= INTERRUPT =======
void IR_ISR() {
  pulseCount++;
}

// ======= HELPERS =======
String urlEncode(const String &str) {
  String encoded = "";
  char c;
  for (size_t i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += '%';
      char buf[3];
      sprintf(buf, "%02X", (uint8_t)c);
      encoded += buf;
    }
  }
  return encoded;
}

// Calculate speed (m/s) from pulses counted in interval_ms
float calculateSpeed(unsigned long pulses, unsigned long interval_ms) {
  if (interval_ms == 0) return 0.0;
  float rotations = (float)pulses / (float)PULSES_PER_REV;
  float circumference = 3.14159265 * WHEEL_DIAMETER_M; // meters
  float meters = rotations * circumference;
  float seconds = interval_ms / 1000.0;
  float mps = meters / seconds;
  return mps; // meters per second
}

void pushReadingToFirebase(float current_mps, float average_mps) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String host = String("https://") + FIREBASE_HOST;
  unsigned long ts = millis();
  // push to /readings as a new object (POST)
  String postUrl = host + "/readings.json?auth=" + FIREBASE_AUTH;
  String payload = "{";
  payload += String("\"timestamp\":") + String(ts) + ",";
  payload += String("\"current_mps\":") + String(current_mps, 3) + ",";
  payload += String("\"average_mps\":") + String(average_mps, 3);
  payload += "}";

  http.begin(postUrl);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload);
  // simple debug prints
  // Serial.println("POST " + postUrl + " => " + String(httpCode));
  http.end();

  // also update /current and /average (PUT)
  String putUrlCur = host + "/current.json?auth=" + FIREBASE_AUTH;
  String putUrlAvg = host + "/average.json?auth=" + FIREBASE_AUTH;

  http.begin(putUrlCur);
  http.addHeader("Content-Type", "application/json");
  http.PUT(String(current_mps,3));
  http.end();

  http.begin(putUrlAvg);
  http.addHeader("Content-Type", "application/json");
  http.PUT(String(average_mps,3));
  http.end();
}

// ======= WiFi portal =======
String portalPage = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width,initial-scale=1" />
    <title>WiFi Setup</title>
  </head>
  <body>
    <h2>ESP32 WiFi Setup</h2>
    <form method="POST" action="/save">
      SSID:<br><input name="ssid" maxlength="32"><br>
      Password:<br><input name="pass" maxlength="64"><br>
      <br><input type="submit" value="Save">
    </form>
  </body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", portalPage);
}

void handleSave() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (ssid.length() > 0) {
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    String resp = "Saved. Rebooting...";
    server.send(200, "text/plain", resp);
    delay(500);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "SSID required");
  }
}

void startPortal() {
  WiFi.mode(WIFI_AP);
  String apName = "Anemometer-Setup-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  WiFi.softAP(apName.c_str());
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

// ======= TFT helpers =======
void tftInit() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
}

void tftShowReading(float current_mps, float avg_mps) {
  tft.fillRect(0,0, tft.width(), 40, TFT_NAVY); // header area (user color)
  tft.setCursor(6,6);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.print("Anemometer");

  tft.setCursor(6, 50);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  float current_kmh = current_mps * 3.6;
  float avg_kmh = avg_mps * 3.6;
  tft.printf("Curr: %.2f km/h\n", current_kmh);
  tft.printf("Avg:  %.2f km/h\n", avg_kmh);
}

// ======= setup & loop =======
void setup() {
  Serial.begin(115200);
  delay(50);
  prefs.begin("anemometer", false);

  // attach interrupt
  pinMode(IR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_PIN), IR_ISR, FALLING);

  tftInit();
  tft.println("Booting...");

  // Try to connect to stored WiFi
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  if (ssid.length() > 0) {
    tft.println("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while (millis() - start < 10000) {
      if (WiFi.status() == WL_CONNECTED) break;
      delay(200);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    // start portal
    tft.println("Starting WiFi portal...\nConnect to AP to configure WiFi");
    startPortal();
  } else {
    tft.println("WiFi connected");
    tft.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  }

  lastSampleMillis = millis();
}

void loop() {
  // handle portal if running
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
  }

  unsigned long now = millis();
  if (now - lastSampleMillis >= SAMPLE_INTERVAL_MS) {
    // take snapshot of pulses atomically
    noInterrupts();
    unsigned long pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    float current_mps = calculateSpeed(pulses, now - lastSampleMillis);

    // update sliding window
    speedWindow[speedWindowIdx] = current_mps;
    speedWindowIdx = (speedWindowIdx + 1) % AVERAGE_WINDOW;
    if (speedWindowCount < AVERAGE_WINDOW) speedWindowCount++;
    float sum = 0;
    for (int i = 0; i < speedWindowCount; i++) sum += speedWindow[i];
    float avg_mps = speedWindowCount ? sum / speedWindowCount : 0;

    // display
    tft.showImage(0,0); // optional placeholder if using images; safe to ignore if not used
    tftShowReading(current_mps, avg_mps);

    // upload
    if (WiFi.status() == WL_CONNECTED) {
      pushReadingToFirebase(current_mps, avg_mps);
    }

    lastSampleMillis = now;
  }
}

--- index.html ---
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Anemometer Dashboard</title>
  <link rel="stylesheet" href="style.css">
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
  <header>
    <h1>Anemometer Dashboard</h1>
    <p id="status">Connecting...</p>
  </header>
  <main>
    <section class="cards">
      <div class="card">
        <h2>Current</h2>
        <p id="current">-- km/h</p>
      </div>
      <div class="card">
        <h2>Average</h2>
        <p id="average">-- km/h</p>
      </div>
    </section>
    <section>
      <canvas id="speedChart" height="120"></canvas>
    </section>
  </main>

  <script>
    const FIREBASE_HOST = 'https://wind-speed-a5223-default-rtdb.firebaseio.com';
    const FIREBASE_AUTH = 'zzB9UhxmW3lt6YXzAgPQQvHfT4d3s55UCLRBpFpG';

    const currentEl = document.getElementById('current');
    const avgEl = document.getElementById('average');
    const statusEl = document.getElementById('status');

    const ctx = document.getElementById('speedChart').getContext('2d');
    const chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: [{
          label: 'Speed (km/h)',
          data: [],
          tension: 0.3,
          fill: false,
          borderWidth: 2,
        }]
      },
      options: {
        scales: {
          x: { display: true },
          y: { beginAtZero: true }
        }
      }
    });

    // helper to fetch JSON from RTDB
    async function fetchJSON(path) {
      const url = `${FIREBASE_HOST}${path}.json?auth=${FIREBASE_AUTH}`;
      const res = await fetch(url);
      if (!res.ok) throw new Error('Network error');
      return res.json();
    }

    async function updateOnce() {
      try {
        statusEl.textContent = 'Loading...';
        const cur = await fetchJSON('/current');
        const avg = await fetchJSON('/average');
        if (cur !== null) currentEl.textContent = (cur * 3.6).toFixed(2) + ' km/h';
        if (avg !== null) avgEl.textContent = (avg * 3.6).toFixed(2) + ' km/h';

        // fetch last ~50 readings
        const readings = await fetchJSON('/readings?orderBy="timestamp"');
        // readings is an object of pushed items
        const list = [];
        for (const k in readings) {
          if (readings[k] && readings[k].timestamp && readings[k].current_mps !== undefined) {
            list.push(readings[k]);
          }
        }
        // sort by timestamp
        list.sort((a,b)=>a.timestamp-b.timestamp);
        // limit to last 50
        const last = list.slice(-50);
        chart.data.labels = last.map(r=> new Date(r.timestamp).toLocaleTimeString());
        chart.data.datasets[0].data = last.map(r=> (r.current_mps*3.6).toFixed(2));
        chart.update();

        statusEl.textContent = 'Live';
      } catch(err) {
        statusEl.textContent = 'Error: ' + err.message;
      }
    }

    // poll every 5 seconds
    updateOnce();
    setInterval(updateOnce, 5000);
  </script>
</body>
</html>

--- style.css ---
body{font-family:Inter, system-ui, Arial; margin:0; background:#0a1f44; color:#fff}
header{padding:16px; background:#081730}
header h1{margin:0;font-size:20px}
.cards{display:flex;gap:12px;padding:12px}
.card{background:#12263f;padding:12px;border-radius:8px;flex:1}
.card h2{margin:0 0 8px 0}
main{padding:12px}

--- END ---
