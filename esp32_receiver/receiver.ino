#include <WiFi.h>
#include <esp_now.h>

typedef struct {
  float voltage;
  float current;
  uint32_t seq;
} Packet;


// ---- Core v3.x signature ----
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  Packet p{};
  memcpy(&p, data, min(len, (int)sizeof(p)));

  const uint8_t *mac = info->src_addr;
  Serial.print("From ");
  for (int i = 0; i < 6; i++) { if (i) Serial.print(":"); Serial.print(mac[i], HEX); }
  Serial.print(" | V="); Serial.print(p.voltage, 2);
  Serial.print(" | A="); Serial.print(p.current, 3);
  Serial.print(" | seq="); Serial.println(p.seq);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); while (1) {} }
  esp_now_register_recv_cb(onRecv);
}

void loop() {
  // if you need to get the MAC
  // Serial.print("Receiver MAC: ");
  // Serial.println(WiFi.macAddress());
}
