#include <WiFi.h>
#include <esp_now.h>

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
const float ADC_REF_VOLT = 3.3;        // with ADC_11db
const int   ADC_MAX      = 4095;
const int   SAMPLES      = 64;

// --- voltage divider (blue 0–25V board ~5:1) ---
const float DIVIDER_RATIO = 5.0;

// --- ACS712 5A @ 3.3V ---
const float ACS_ZERO_V   = 3.3f / 2.0f;             // ~1.65 V center
const float ACS_MV_PER_A = 185.0f * (3.3f / 5.0f);  // ≈ 122.1 mV/A
int CURRENT_SIGN = +1;                               // flip to -1 if sign is backwards

uint32_t seq = 0;

float readAdcVolts(int pin, int samples = SAMPLES) {
  uint32_t acc = 0;
  for (int i = 0; i < samples; i++) { acc += analogRead(pin); delayMicroseconds(80); }
  return (acc / (float)samples) * ADC_REF_VOLT / ADC_MAX;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);  // ~0–3.3V

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

  // Read current (ACS712 5A @3.3V)
  float v_cur  = readAdcVolts(CURRENT_PIN);                 // 0..3.3V direct
  float amps   = ((v_cur - ACS_ZERO_V) * 1000.0f) / ACS_MV_PER_A;
  amps *= CURRENT_SIGN;

  Packet p{ v_batt, amps, ++seq };
  esp_now_send(peerMac, reinterpret_cast<const uint8_t*>(&p), sizeof(p));

  Serial.print("Sent  V="); Serial.print(v_batt, 2);
  Serial.print("  A=");     Serial.print(amps, 3);
  Serial.print("  seq=");   Serial.println(seq);
  float v_cur2  = readAdcVolts(CURRENT_PIN);  // 0..3.3V at ESP32 pin
  Serial.print("v_cur="); Serial.println(v_cur2, 3);


  delay(1000); // or 5000 if you prefer
}
