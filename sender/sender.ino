#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP32Servo.h>

Servo watts_servo;
Servo amps_servo;
Servo volts_servo;

Adafruit_INA219 ina219;

// Contols how often inputs are checked
unsigned long lastInputCheck = 0;
const unsigned long inputInterval = 300; 

// Configuration for scaling and movement speed
float max_pos   = 170;  // Max angle for servo movement
float amp_slew  = 1;    // Step size for current needle movement
float volt_slew = 1;    // Step size for voltage needle movement
float watt_slew = 1;    // Step size for power needle movement

// Current displayed positions for each servo
float curr_pos_volts = 80;
float curr_pos_amps  = 80;
float curr_pos_watts = 80;

// Fluctuation flags — determines if needles should visually "jitter"
bool fluctuate_volts = false;
bool fluctuate_amps  = false;
bool fluctuate_watts = false;

// ---------- PUT YOUR RECEIVER MAC HERE ----------
// Receiver's STA MAC:
uint8_t peerMac[6] = { 0x14, 0x33, 0x5C, 0x38, 0x9B, 0x38 };


// --- payload (MUST match receiver) ---
typedef struct {
  float voltage;   // V
  float current;   // A
  uint32_t seq;
} Packet;

// --- pins & ADC config ---
const int VOLTAGE_PIN = 33;            // blue divider S -> ADC1
const int CURRENT_PIN = 36;            // ACS712 OUT -> ADC1 (direct, 3.3V)
const float ADC_REF_VOLT = 3.3f;        // with ADC_11db
const int   ADC_MAX      = 4095;
const int   SAMPLES      = 64;

// --- voltage divider (blue 0–25V board ~5:1) ---
const float DIVIDER_RATIO = 5.0;

// --- ACS712 5A @ 3.3V ---
const float ACS_ZERO_V   = 3.3f / 2.0f;             // ~1.65 V center
const float ACS_MV_PER_A = 185.0f * (3.3f / 5.0f);  // ≈ 122.1 mV/A
int CURRENT_SIGN = +1;                               // flip to -1 if sign is backwards

uint32_t seq = 0;

const int AVG_SIZE = 5;
float voltageHistory[AVG_SIZE] = {0};
float currentHistory[AVG_SIZE] = {0};
int avgIndex = 0;
int sampleCount = 0;

float readAdcVolts(int pin, int samples = SAMPLES) {
  uint32_t acc = 0;
  for (int i = 0; i < samples; i++) { acc += analogRead(pin); delayMicroseconds(80); }
  return (acc / (float)samples) * ADC_REF_VOLT / ADC_MAX;
}

void updateRollingAverage(float newVoltage, float newCurrent, float &avgVoltage, float &avgCurrent) {
  // store new readings into circular buffer
  voltageHistory[avgIndex] = newVoltage;
  currentHistory[avgIndex] = newCurrent;

  avgIndex = (avgIndex + 1) % AVG_SIZE;
  if (sampleCount < AVG_SIZE) sampleCount++;

  // compute average
  float vSum = 0, cSum = 0;
  for (int i = 0; i < sampleCount; i++) {
    vSum += voltageHistory[i];
    cSum += currentHistory[i];
  }

  avgVoltage = vSum / sampleCount;
  avgCurrent = cSum / sampleCount;
}

void setup() {

  // Attach each servo object to ESP32 PWM-capable GPIOs
  watts_servo.attach(25);  // was 9
  amps_servo.attach(26);   // was 10
  volts_servo.attach(27);  // was 11

  // Initial motion to show all needles moving full range
  watts_servo.write(max_pos);
  amps_servo.write(0);
  volts_servo.write(max_pos);
  delay(1000);

  // Move all needles to the neutral starting position
  watts_servo.write(curr_pos_watts);
  amps_servo.write(curr_pos_amps);
  volts_servo.write(curr_pos_volts);

  Serial.begin(115200);
  Wire.begin(21, 22);   // SDA=21, SCL=22 on ESP32
  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
  ina219.setCalibration_32V_2A();  // good default: up to 32V bus, 2A current
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); while (1) {} }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, peerMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  peer.ifidx = WIFI_IF_STA; // required in core v3.x
#endif
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("Add peer failed"); while (1) {} }
}

void loop() {
  if (millis() - lastInputCheck >= inputInterval) {
  lastInputCheck = millis();
  // Read battery voltage
  float v_adc  = readAdcVolts(VOLTAGE_PIN);
  Serial.print(v_adc, 2);
  float v_batt = v_adc * DIVIDER_RATIO;
  float curr_A = ina219.getCurrent_mA() / 1000.0f;
  curr_A *= CURRENT_SIGN;

  // compute rolling average
  float avgV = 0, avgC = 0;
  updateRollingAverage(v_batt, curr_A, avgV, avgC);
  
  Packet p{ avgV, avgC, ++seq };
  esp_now_send(peerMac, reinterpret_cast<const uint8_t*>(&p), sizeof(p));

  checkInput(avgV, avgC);

  if (fluctuate_volts || fluctuate_amps || fluctuate_watts) {
    fluctuateNeedles();
  }
  
  Serial.print("Sent  V="); Serial.print(v_batt, 2);
  Serial.print("  Current="); Serial.print(curr_A, 2);
  Serial.print("  AvgV="); Serial.print(avgV, 2);
  Serial.print("  AvgC="); Serial.print(avgC, 2);
  Serial.print("  seq="); Serial.println(seq);
  }
}

void checkInput(float voltage, float amps) {

  float watts   = voltage * amps;                   // Calculate power

  // Print values for debugging
  Serial.print(voltage);
  Serial.print(" ");
  Serial.print(amps);
  Serial.print("\n");

  // Convert measurements to servo angle (inverted to match needle direction)
  float volts_pos = max_pos - (voltage / 20.0) * max_pos;
  float amps_pos  = max_pos - (amps / 0.95)   * max_pos;
  float watts_pos = max_pos - (watts / 14.5)  * max_pos;

  // Ensure angles stay within servo movement range
  volts_pos = constrain(volts_pos, 0, max_pos);
  amps_pos  = constrain(amps_pos,  0, max_pos);
  watts_pos = constrain(watts_pos, 0, max_pos);

  // Check how much the new input changed from current position
  float volts_delta = abs(volts_pos - curr_pos_volts);
  float amps_delta  = abs(amps_pos  - curr_pos_amps);
  float watts_delta = abs(watts_pos - curr_pos_watts);

  // If change is small, enable fluctuation effect
  fluctuate_volts = (volts_delta < 1.0);
  fluctuate_amps  = (amps_delta  < 1.0);
  fluctuate_watts = (watts_delta < 1.0);

  // Smoothly move servos toward new target positions
  updateServos(watts_pos, amps_pos, volts_pos);
}

// Moves each servo gradually toward its target position
void updateServos(float target_pos_watts, float target_pos_amps, float target_pos_volts) {
  // Decide direction of movement for each servo
  float step_w = (target_pos_watts > curr_pos_watts) ? watt_slew : -watt_slew;
  float step_a = (target_pos_amps  > curr_pos_amps)  ? amp_slew  : -amp_slew;
  float step_v = (target_pos_volts > curr_pos_volts) ? volt_slew : -volt_slew;

  // Update watts servo if far from target
  if (abs(curr_pos_watts - target_pos_watts) > 1) {
    curr_pos_watts += step_w;
    curr_pos_watts = constrain(curr_pos_watts, 0, max_pos);
    watts_servo.write(curr_pos_watts);
  }

  // Update amps servo
  if (abs(curr_pos_amps - target_pos_amps) > 1) {
    curr_pos_amps += step_a;
    curr_pos_amps = constrain(curr_pos_amps, 0, max_pos);
    amps_servo.write(curr_pos_amps);
  }

  // Update volts servo
  if (abs(curr_pos_volts - target_pos_volts) > 1) {
    curr_pos_volts += step_v;
    curr_pos_volts = constrain(curr_pos_volts, 0, max_pos);
    volts_servo.write(curr_pos_volts);
  }
}

// Adds occasional random "spikes" to simulate analog fluctuations
void fluctuateNeedles() {
  static unsigned long last_update = 0;

  if (millis() - last_update >= 40) {
    last_update = millis();

    // Occasionally apply a random spike to each servo independently
    if (fluctuate_volts && random(0, 100) < 7) {
      float spike = random(-3, 3);  // ±6 degree spike
      volts_servo.write(constrain(curr_pos_volts + spike, 0, max_pos));
    }

    if (fluctuate_amps && random(0, 100) < 7) {
      float spike = random(-3, 3);
      amps_servo.write(constrain(curr_pos_amps + spike, 0, max_pos));
    }

    if (fluctuate_watts && random(0, 100) < 3) {
      float spike = random(-1, 1);
      watts_servo.write(constrain(curr_pos_watts + spike, 0, max_pos));
    }
  }
}
