# Anemometer
#include <Servo.h>

// ========================== CONFIG ==========================
#define USE_INTERNAL_HALL true     // Change to false if using external sensor
#define EXTERNAL_HALL_PIN 4        // External hall pin (if used)
#define SERVO_PIN 3                // Servo pin

Servo servo;
volatile unsigned int rotationCount = 0;
unsigned long lastTime = 0;
float windSpeed = 0.0;

// ========================== SETUP ==========================
void setup() {
  Serial.begin(115200);
  servo.attach(SERVO_PIN);

  if (!USE_INTERNAL_HALL) {
    pinMode(EXTERNAL_HALL_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(EXTERNAL_HALL_PIN), countRotation, FALLING);
    Serial.println("Using external Hall sensor...");
  } else {
    Serial.println("Using internal Hall sensor...");
  }
}

// ========================== MAIN LOOP ==========================
void loop() {
  unsigned long currentTime = millis();

  if (USE_INTERNAL_HALL) {
    int hallValue = hallRead();  // Internal hall sensor value
    Serial.print("Hall Value: ");
    Serial.println(hallValue);

    // Simulated pulse counting based on threshold
    if (hallValue > 20 || hallValue < -20) {
      rotationCount++;
      delay(50); // debounce
    }
  }

  // Calculate every 2 seconds
  if (currentTime - lastTime >= 2000) {
    float timeInSeconds = (currentTime - lastTime) / 1000.0;
    float rotationsPerSec = rotationCount / timeInSeconds;

    // radius = 0.05 m (modify as needed)
    windSpeed = 2 * 3.1416 * 0.05 * rotationsPerSec;

    // Reset counters
    rotationCount = 0;
    lastTime = currentTime;

    // Map wind speed (0–20 m/s) → Servo (0–180°)
    int angle = map(constrain(windSpeed, 0, 20), 0, 20, 0, 180);
    servo.write(angle);

    Serial.print("Wind Speed: ");
    Serial.print(windSpeed);
    Serial.println(" m/s");
  }
}

// ========================== ISR ==========================
void countRotation() {
  rotationCount++;
}
