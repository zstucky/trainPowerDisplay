#include <Servo.h>

// Servo objects for each gauge
Servo watts_servo;
Servo amps_servo;
Servo volts_servo;

// Constants for scaling and slew rate
float max_pos   = 170;  // Max servo angle
float amp_slew  = 2;    // Amps servo movement step
float volt_slew = 2;    // Volts servo movement step
float watt_slew = 2;    // Watts servo movement step

// Current positions of each servo
float curr_pos_volts = 80;
float curr_pos_amps  = 80;
float curr_pos_watts = 80;

// Previous target positions (used to detect if input is stable)
float prev_volts_pos = 0;
float prev_amps_pos  = 0;
float prev_watts_pos = 0;

// Fluctuation flags for each servo (indicates if it should jitter)
bool fluctuate_volts = false;
bool fluctuate_amps  = false;
bool fluctuate_watts = false;

void setup() {
  // Attach each servo to a pin
  watts_servo.attach(9);
  amps_servo.attach(10);
  volts_servo.attach(11);

  // Quick startup sweep to show all needles moving
  watts_servo.write(max_pos);
  amps_servo.write(0);
  volts_servo.write(max_pos);
  delay(1000);

  // Move to neutral position
  watts_servo.write(curr_pos_watts);
  amps_servo.write(curr_pos_amps);
  volts_servo.write(curr_pos_volts);

  // Start serial monitor
  Serial.begin(115200);
  Serial.println("Enter: volts amps (e.g. 5.0 2.0)");
}

void loop() {
  // Continuously check for input from Serial Monitor
  checkInput();

  // Only fluctuate servos that have stable input
  if (fluctuate_volts || fluctuate_amps) {
    fluctuateNeedles();
  }
}

// Reads user input from serial and updates servo targets
void checkInput() {
  if (Serial.available()) {
    float voltage = Serial.parseFloat();  // First value: volts
    float amps    = Serial.parseFloat();  // Second value: amps
    float watts   = voltage * amps;       // Derived value

    // Wait until a newline to confirm input completion
    if (Serial.read() == '\n') {
      // Convert voltage, amps, watts into servo angles
      float volts_pos = max_pos - (voltage / 20.0) * max_pos;
      float amps_pos  = max_pos - (amps / 1.0)   * max_pos;
      float watts_pos = max_pos - (watts / 14.5) * max_pos;

      // Clamp angles to servo limits
      volts_pos = constrain(volts_pos, 0, max_pos);
      amps_pos  = constrain(amps_pos,  0, max_pos);
      watts_pos = constrain(watts_pos, 0, max_pos);

      // Compare to previous values to decide whether to fluctuate
      float volts_delta = abs(volts_pos - curr_pos_volts);
      float amps_delta  = abs(amps_pos  - curr_pos_amps);

      fluctuate_volts = (volts_delta < 3.0);  // If change is small, fluctuate
      fluctuate_amps  = (amps_delta  < 3.0);

      // Save as previous values for next loop
      prev_volts_pos = volts_pos;
      prev_amps_pos  = amps_pos;
      prev_watts_pos = watts_pos;

      // Move the servos to new target positions
      updateServos(watts_pos, amps_pos, volts_pos);
    }
  }
}

// Moves servos toward their target positions in small steps
void updateServos(float target_pos_watts, float target_pos_amps, float target_pos_volts) {
  // Determine which direction to move each servo
  float step_w = (target_pos_watts > curr_pos_watts) ? watt_slew : -watt_slew;
  float step_a = (target_pos_amps  > curr_pos_amps)  ? amp_slew  : -amp_slew;
  float step_v = (target_pos_volts > curr_pos_volts) ? volt_slew : -volt_slew;

  // Smoothly move watts servo
  if (abs(curr_pos_watts - target_pos_watts) > 3) {
    curr_pos_watts += step_w;
    curr_pos_watts = constrain(curr_pos_watts, 0, max_pos);
    watts_servo.write(curr_pos_watts);
  }

  // Smoothly move amps servo
  if (abs(curr_pos_amps - target_pos_amps) > 3) {
    curr_pos_amps += step_a;
    curr_pos_amps = constrain(curr_pos_amps, 0, max_pos);
    amps_servo.write(curr_pos_amps);
  }

  // Smoothly move volts servo (only if not near target)
  if (abs(curr_pos_volts - target_pos_volts) > 3) {
    curr_pos_volts += step_v;
    curr_pos_volts = constrain(curr_pos_volts, 0, max_pos);
    volts_servo.write(curr_pos_volts);
  }
}

// Adds small jitter to each servo that has stable input
void fluctuateNeedles() {
  static unsigned long last_move = 0;     // Time of last movement
  const unsigned long fluctuate_delay = 100; // Delay between jitter frames
  static float phase = 0;                 // Controls sine wave

  if (millis() - last_move >= fluctuate_delay) {
    last_move = millis();

    // Jitter pattern using sine wave
    float jitter = sin(phase) * 1.5;  // ~±1.5 degree movement
    phase += 0.3;
    if (phase > TWO_PI) phase = 0;

    // Apply fluctuation to each servo independently
    if (fluctuate_volts) {
      volts_servo.write(constrain(curr_pos_volts + jitter, 0, max_pos));
    }
    if (fluctuate_amps) {
      amps_servo.write(constrain(curr_pos_amps + jitter, 0, max_pos));
    }
    if (fluctuate_volts || fluctuate_amps) {
      watts_servo.write(constrain(curr_pos_watts + jitter, 0, max_pos));
    }
  }
}
