#include <WiFi.h>
#include <esp_now.h>

typedef struct {
  float voltage;
  float current;
  uint32_t seq;
} Packet;

// -------- ring buffer config --------
static const int BUF_SIZE = 10;

// Last 10 readings (circular)
float voltBuf[BUF_SIZE] = {0};
float ampBuf[BUF_SIZE]  = {0};
uint32_t seqBuf[BUF_SIZE] = {0};  // optional, if you want to track seq too

// Head points to the NEXT write position
volatile int bufHead = 0;
// Count of valid samples (0..BUF_SIZE). Helpful for first 10 packets.
volatile int bufCount = 0;

// (Optional) most recent values, if you still want quick access
volatile float lastVoltage = 0.0f;
volatile float lastCurrent = 0.0f;
volatile uint32_t lastSeq = 0;

// ---- ESP-NOW receive callback ----
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  Packet p{};
  memcpy(&p, data, min(len, (int)sizeof(p)));

  // Store latest
  lastVoltage = p.voltage;
  lastCurrent = p.current;
  lastSeq     = p.seq;

  // Push into ring buffer
  voltBuf[bufHead] = p.voltage;
  ampBuf[bufHead]  = p.current;
  seqBuf[bufHead]  = p.seq;

  bufHead = (bufHead + 1) % BUF_SIZE;
  if (bufCount < BUF_SIZE) bufCount++;

  // (optional) quick confirmation
  Serial.print("Stored -> V="); Serial.print(p.voltage, 2);
  Serial.print(" A="); Serial.print(p.current, 3);
  Serial.print(" seq="); Serial.print(p.seq);
  Serial.print(" | bufCount="); Serial.println(bufCount);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1) {}
  }
  esp_now_register_recv_cb(onRecv);
}

void loop() {
  // Example: print the buffer from newest → oldest every 2 seconds
  static unsigned long t0 = 0;
  if (millis() - t0 > 2000) {
    t0 = millis();

    Serial.println("--- Last readings (newest → oldest) ---");
    int n = bufCount;                         // how many valid samples we have
    for (int i = 0; i < n; i++) {
      // newest is just before bufHead; walk backwards
      int idx = (bufHead - 1 - i + BUF_SIZE) % BUF_SIZE;
      Serial.print("#"); Serial.print(i);
      Serial.print("  V="); Serial.print(voltBuf[idx], 2);
      Serial.print("  A="); Serial.print(ampBuf[idx], 3);
      Serial.print("  seq="); Serial.println(seqBuf[idx]);
    }
    Serial.println("---------------------------------------");
  }
}
