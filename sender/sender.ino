#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_INA219.h>


Adafruit_INA219 ina219;

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
  // Read battery voltage
  float v_adc  = readAdcVolts(VOLTAGE_PIN);
  float v_batt = v_adc * DIVIDER_RATIO;
  float curr_mA   = ina219.getCurrent_mA();
  curr_mA *= CURRENT_SIGN;

  // compute rolling average
  float avgV = 0, avgC = 0;
  updateRollingAverage(v_batt, curr_mA, avgV, avgC);

  Packet p{ v_batt, curr_mA, ++seq };
  esp_now_send(peerMac, reinterpret_cast<const uint8_t*>(&p), sizeof(p));

  int raw = analogRead(36);  
  float test  = raw * 3.3f / 4095.0f;
  
  Serial.print("Sent  V="); Serial.print(v_batt, 2);
  Serial.print("  Current="); Serial.print(curr_mA, 2);
  Serial.print("  AvgV="); Serial.print(avgV, 2);
  Serial.print("  AvgC="); Serial.print(avgC, 2);
  Serial.print("  seq="); Serial.println(seq);

  delay(2000); // or 5000 if you prefer
}
