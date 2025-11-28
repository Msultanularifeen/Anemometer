# ESP32 Anemometer Firmware – FGSD Project

This repository contains the firmware for the ESP32-based Anemometer System used in the FGSD BS Physics (2024–2028) project.  
The code measures wind speed, displays it on a TFT screen, and uploads real-time + historical data to Firebase.  
The main dashboard is hosted on Vercel: [Live Dashboard](https://anemometer-m6bya21wf-msultanularifeens-projects.vercel.app/)

---

## 📌 Features
- IR-based pulse counting for wind measurement  
- Calculates real wind speed using diameter & pulse parameters  
- TFT display with clean UI  
- Live uploads to Firebase Realtime Database  
- Automatic cleanup of old history data  
- Smooth animated speed transitions  
- NTP time synchronization (UTC+5 Pakistan)

---

## 🛠️ Hardware Used
- **ESP32**
- **IR Sensor**
- **Anemometer Rotor**
- **TFT Display (TFT_eSPI)**
- **WiFi Network**

---

## 🔌 Wiring Diagram (ESP32)
- **IR Sensor**
  - VCC → 3.3V  
  - GND → GND  
  - Signal → GPIO 21 (used in code as `IR_PIN`)  
- **TFT Display**
  - Connect according to `TFT_eSPI` library configuration (CS, DC, RST, MOSI, MISO, SCK, 3.3V, GND)  
- ESP32 powered via USB or 5V regulated supply  
> Make sure IR sensor is positioned properly to detect anemometer rotations.  

---

## 📡 Cloud Services
- **Firebase Realtime Database** – stores latest + historical readings  
- **Vercel Hosting** – displays real-time dashboard for viewers  

Special appreciation to Firebase and Vercel for supporting this project.

---

## 📂 Code Overview
**Main functions include:**
- `IR_ISR()` → Pulse counting  
- `calculateSpeedFromPulses()` → Wind speed calculation  
- `drawSpeedStatic()` → UI rendering on TFT  
- `uploadToFirebase()` → Realtime + history upload  
- `cleanOldData()` → Auto-delete older than 24 hours  
- WiFi & NTP initialization  
- Smooth UI animation for speed

---

## ⚙️ Configuration
Inside the code:
- WiFi SSID & Password  
- Firebase Host & Auth Token  
- Anemometer diameter  
- Pulses per revolution  
- Update interval (15 seconds)  
- Time server & timezone settings  

---

## 🚀 How It Works
1. The IR sensor detects rotations of the anemometer cups.  
2. ESP32 counts pulses using an interrupt.  
3. Every 15 seconds:
   - Calculates wind speed (m/s)  
   - Sends data to Firebase  
   - Updates TFT display  
4. Firebase history is stored for charting and cleaned every hour.  
5. Vercel-hosted website reads Firebase data and shows real-time graphs.

---

## 👨‍💻 Developer
**Muhammad Sultan Ul Arifeen**  
Planning • Electrical • Programming • Designing • Development

## 🔧 Mechanical Work  
**Masoom Ali**

---

## 🙌 Acknowledgments
Thanks to **Firebase** for the realtime database  
and **Vercel** for fast and reliable hosting.

---

## 🌐 Live Website
[View Live Dashboard](https://anemometer-m6bya21wf-msultanularifeens-projects.vercel.app/)
