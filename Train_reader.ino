#include <ESP32Servo.h>

Servo watts_servo;
Servo amps_servo;
Servo volts_servo;

// Analog input pins for potentiometers simulating voltage and current (ESP32 ADC1)
int volt_pot_pin = 33;   // was A4
int amp_pot_pin  = 32;   // was A5
int volt_pot_value = 0;
int amp_pot_value = 0;

// Contols how often inputs are checked
unsigned long lastInputCheck = 0;
const unsigned long inputInterval = 15; 

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

void setup() {
  // Keep your 10-bit math (0..1023) on ESP32 so your scaling stays unchanged
  analogReadResolution(10);

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

  // Start serial communication for debugging or monitoring
  Serial.begin(115200);
}

void loop() {
  // Check inputs periodically based on inputInterval
  if (millis() - lastInputCheck >= inputInterval) {
    lastInputCheck = millis();
    checkInput();  // Reads potentiometer values and updates targets
  }

  // Add visual fluctuations if any value has been stable recently
  if (fluctuate_volts || fluctuate_amps || fluctuate_watts) {
    fluctuateNeedles();
  }
}

// Reads potentiometer inputs and calculates target servo positions
void checkInput() {
  volt_pot_value = analogRead(volt_pot_pin);  // was analogRead(A4);
  amp_pot_value  = analogRead(amp_pot_pin);   // was analogRead(A5);

  // Map to real-world units (volts and amps)
  float voltage = (volt_pot_value / 1023.0) * 20.0;  // Assume 0–20V range
  float amps    = (amp_pot_value  / 1023.0) * 0.95;  // Assume 0–0.95A range
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
