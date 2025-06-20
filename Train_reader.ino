#include <Servo.h>

Servo watts_servo;
Servo amps_servo;
Servo volts_servo;

// Direction control flag for fluctuate
bool fluctuate = true;

// Configuration constants and current positions
float max_pos = 170;       // Maximum servo angle
float amp_slew = 5;        // Step size for amps servo movement
float volt_slew = 5;       // Step size for volts servo movement
float watt_slew = 5;       // Step size for watts servo movement

// Current servo positions (updated over time)
float curr_pos_volts;
float curr_pos_amps;
float curr_pos_watts;

void setup() {
  // Attach each servo to its corresponding pin
  watts_servo.attach(9);
  amps_servo.attach(10);
  volts_servo.attach(11);
  
  // Initial sweep motion to test servos
  watts_servo.write(max_pos);
  amps_servo.write(0);
  volts_servo.write(max_pos);
  delay(1000);
  watts_servo.write(80);
  amps_servo.write(80);
  volts_servo.write(80);

  // Initialize current position tracking
  curr_pos_volts = 80;
  curr_pos_amps = 80;
  curr_pos_watts = 80;
  
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("Enter: volts amps (e.g. 5 2)");
}

void loop() {
  // Check for new input from Serial
  checkInput();

  // If no movement is in progress, animate voltage servo slightly
  if (fluctuate) {
    fluctuateVoltage();
  }
}

void checkInput() {
  // If data is available on the Serial port
  if (Serial.available()) {
    // Parse float values for voltage and amps
    float voltage = Serial.parseFloat();
    float amps    = Serial.parseFloat();
    float watts   = voltage * amps;

    // Convert input values into servo target positions
    float volts_pos = max_pos - (voltage / 20.0) * max_pos;
    float amps_pos  = max_pos - (amps / 1.0) * max_pos;
    float watts_pos = max_pos - (watts / 14.5) * max_pos;

    // Wait for newline character to confirm full input
    if (Serial.read() == '\n') {
      // Clamp values within valid servo range
      volts_pos = constrain(volts_pos, 0, max_pos);
      amps_pos  = constrain(amps_pos, 0, max_pos);
      watts_pos = constrain(watts_pos, 0, max_pos);

      // Pass target positions to servo update function
      updateServos(watts_pos, amps_pos, volts_pos);
    }
  }
}

void fluctuateVoltage() {
  static unsigned long last_move = 0;             // Timestamp of last servo move
  const unsigned long fluctuate_delay = 125;      // Delay between fluctuations
  static bool fluctuate_direction = true;         // Direction of movement
  static float fluctuate_offset = 0;              // Current offset from center

  // Causes Delay
  if (millis() - last_move >= fluctuate_delay) {
    last_move = millis();

    // Increment or decrement offset depending on direction
    if (fluctuate_direction) {
      fluctuate_offset += 1;
    } else {
      fluctuate_offset -= 1;
    }

    // Change direction at offset bounds
    if (fluctuate_offset >= 3) {
      fluctuate_offset = 3;
      fluctuate_direction = false;
    } else if (fluctuate_offset <= -3) {
      fluctuate_offset = -3;
      fluctuate_direction = true;
    }

    // Apply fluctuated offset to voltage and watt servos
    volts_servo.write(constrain(curr_pos_volts + fluctuate_offset, 0, max_pos));
    watts_servo.write(constrain(curr_pos_watts + fluctuate_offset, 0, max_pos));
  }
}

void updateServos(int target_pos_watts, int target_pos_amps, int target_pos_volts) {
  // Determine movement direction for each servo
  float step_w = (target_pos_watts > curr_pos_watts) ? watt_slew : -watt_slew;
  float step_a = (target_pos_amps > curr_pos_amps) ? amp_slew : -amp_slew;
  float step_v = (target_pos_volts > curr_pos_volts) ? volt_slew : -volt_slew;

  // Update watts servo if not yet at target
  if (curr_pos_watts != target_pos_watts) {
    curr_pos_watts += step_w;
    watts_servo.write(curr_pos_watts);
  }

  // Update amps servo if not yet at target
  if (curr_pos_amps != target_pos_amps) {
    curr_pos_amps += step_a;
    amps_servo.write(curr_pos_amps);
  }

  // Update volts servo if it's more than 3 units away from target
  if (abs(curr_pos_volts - target_pos_volts) > 3) {
    fluctuate = false;  // Turn off fluctuation while moving
    curr_pos_volts += step_v;
    volts_servo.write(curr_pos_volts);
  } 
  else {
    fluctuate = true;   // Resume fluctuation once target is reached
  }
}
