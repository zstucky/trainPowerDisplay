#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>

Servo watts_servo;
Servo amps_servo;
Servo volts_servo;

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

volatile float lastAvgV = 0.0f;   // volts from last packet (already averaged)
volatile float lastAvgA = 0.0f;   // amps  from last packet (already averaged)
volatile uint32_t lastSeq = 0;
volatile bool havePacket = false;
unsigned long lastPktMs = 0;      // (optional) for a stale-link check

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < (int)sizeof(Packet)) return;
  Packet p{};
  memcpy(&p, data, sizeof(Packet));

  lastAvgV  = p.voltage;  // already averaged on sender
  lastAvgA  = p.current;  // already averaged on sender
  lastSeq   = p.seq;
  havePacket = true;
  lastPktMs  = millis();
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
  if (millis() - t0 > 50) {
    t0 = millis();

    if (havePacket) {
      // use the last received averages directly
      checkInput(lastAvgV, lastAvgA);
    }

    // optional: tiny jitter if close to target
    if (fluctuate_volts || fluctuate_amps || fluctuate_watts) {
      fluctuateNeedles();
    }

    // (optional) stale-link behavior: decay toward zero if no packets in 4s
    if (millis() - lastPktMs > 4000 && havePacket) {
      float v = lastAvgV * 0.995f;
      float a = lastAvgA * 0.995f;
      checkInput(v, a);
    }
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
  fluctuate_amps  = (amps_delta  < 0.1);
  fluctuate_watts = (watts_delta < 0.4);

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
  if (abs(curr_pos_watts - target_pos_watts) > 0.4) {
    curr_pos_watts += step_w;
    curr_pos_watts = constrain(curr_pos_watts, 0, max_pos);
    watts_servo.write(curr_pos_watts);
  }

  // Update amps servo
  if (abs(curr_pos_amps - target_pos_amps) > 0.1) {
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
