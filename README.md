# Anemometer

A simple anemometer (wind speed sensor) project using an Arduino-compatible board and a Hall-effect sensor (internal or external). The measured wind speed is converted to a servo angle (0–180°), allowing a visual indicator (or other mechanical response) driven by a servo motor.

## Features

- Supports internal or external Hall-effect sensor
- Calculates wind speed from rotation count
- Maps wind speed to servo angle (0–180°)
- Serial output for debugging and monitoring

## Hardware Required

- Arduino-compatible board (e.g., ESP32, Arduino Uno — note: `hallRead()` used in code is an ESP32 built-in function; change if using other boards)
- Hall-effect sensor (or use an internal Hall sensor on compatible boards)
- Servo motor
- Jumper wires
- Power supply appropriate for the servo and board
- Optional: anemometer rotor with magnet(s)

## Wiring

- Servo signal pin → SERVO_PIN (default in code: 3)
- External Hall sensor output → EXTERNAL_HALL_PIN (default in code: 4) if `USE_INTERNAL_HALL` is set to `false`
- Ground the sensors and servo to the Arduino ground
- Power servo with a stable 5V (or as required by your servo); do not power a high-current servo directly from the Arduino 3.3V/5V pin if it draws more current than the board can supply

## Configuration

Open the sketch and adjust these defines at the top to match your setup:

- `USE_INTERNAL_HALL`: `true` to use the board's internal Hall sensor (ESP32), `false` to use an external Hall sensor connected to `EXTERNAL_HALL_PIN`.
- `EXTERNAL_HALL_PIN`: Pin number for an external Hall sensor (used only when `USE_INTERNAL_HALL` is `false`).
- `SERVO_PIN`: Pin connected to the servo signal.
- Radius used in wind speed calculation is currently set to 0.05 m (5 cm). Adjust if your rotor radius differs.

Notes:
- The code expects pulses (rotations) from a Hall sensor; if using an external sensor, the code attaches an interrupt on a falling edge to count rotations.
- For ESP32 internal hall, the code uses `hallRead()` and uses a simple threshold-based pulse simulation. If you want more accurate pulse detection with internal sensors, consider implementing a more robust filtering approach.

## Usage

- Load the sketch onto your Arduino/ESP32.
- Open the serial monitor (115200 bps) to see hall values (if using internal sensor) and calculated wind speed in m/s.
- The servo angle will update every 2 seconds based on the measured wind speed (mapped from 0–20 m/s to 0–180°).

## Troubleshooting

- If wind speed always reads 0:
  - Verify the Hall sensor wiring.
  - Check that the magnet on the rotor passes the sensor properly.
  - If using the internal Hall sensor, ensure your board actually has one (ESP32 has it).
- If the servo jitters or behaves unpredictably:
  - Use a separate power supply for the servo with a common ground.
  - Add smoothing or a low-pass filter to the measured value before writing to the servo.
- Debounce and noise can cause false counts; adjust thresholds or add hardware filtering if needed.

## License

Include whichever license you prefer for your project (e.g., MIT). This README contains descriptive text only — choose and add a LICENSE file if you want to include an open-source license.

## Arduino Sketch

The full sketch is included below. Copy and paste it into your Arduino IDE (select the correct board and COM port), adjust the configuration at the top if necessary, and upload.

```cpp
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
```
