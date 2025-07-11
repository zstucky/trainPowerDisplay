#include <Servo.h>

// Servo objects for each gauge
Servo watts_servo;
Servo amps_servo;
Servo volts_servo;

int volt_pot_pin = A4;
int amp_pot_pin = A5;
int volt_pot_value = 0;
int amp_pot_value = 0;

unsigned long lastInputCheck = 0;
const unsigned long inputInterval = 15;

// Constants for scaling and slew rate
float max_pos   = 170;  // Max servo angle
float amp_slew  = 1;    // Amps servo movement step
float volt_slew = 1;    // Volts servo movement step
float watt_slew = 1;    // Watts servo movement step

// Current positions of each servo
float curr_pos_volts = 80;
float curr_pos_amps  = 80;
float curr_pos_watts = 80;

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

  if (millis() - lastInputCheck >= inputInterval) {
    lastInputCheck = millis();
    checkInput();  // your existing function
  }
  
  // Only fluctuate servos that have stable input
  if (fluctuate_volts || fluctuate_amps || fluctuate_watts) {
    fluctuateNeedles();
  }
}

// Reads user input from serial and updates servo targets
void checkInput() {
  volt_pot_value = analogRead(A4);
  amp_pot_value = analogRead(A5);
  float voltage = (volt_pot_value / 1023.0) * 20.0;
  float amps = (amp_pot_value / 1023.0) * .95;
  float watts = voltage * amps;
  Serial.print(voltage);
  Serial.print(" ");
  Serial.print(amps);
  Serial.print("\n");
 
  // Convert voltage, amps, watts into servo angles
  float volts_pos = max_pos - (voltage / 20.0) * max_pos;
  float amps_pos  = max_pos - (amps / .95)   * max_pos;
  float watts_pos = max_pos - (watts / 14.5) * max_pos;
  
  // Clamp angles to servo limits
  volts_pos = constrain(volts_pos, 0, max_pos);
  amps_pos  = constrain(amps_pos,  0, max_pos);
  watts_pos = constrain(watts_pos, 0, max_pos);

  // Compare to previous values to decide whether to fluctuate
  float volts_delta = abs(volts_pos - curr_pos_volts);
  float amps_delta  = abs(amps_pos  - curr_pos_amps);
  float watts_delta  = abs(watts_pos  - curr_pos_watts);

  fluctuate_volts = (volts_delta < 1.0);  // If change is small, fluctuate
  fluctuate_amps  = (amps_delta  < 1.0);
  fluctuate_watts  = (watts_delta  < 1.0);

  // Move the servos to new target positions
  updateServos(watts_pos, amps_pos, volts_pos);
    
  }

// Moves servos toward their target positions in small steps
void updateServos(float target_pos_watts, float target_pos_amps, float target_pos_volts) {
  // Determine which direction to move each servo
  float step_w = (target_pos_watts > curr_pos_watts) ? watt_slew : -watt_slew;
  float step_a = (target_pos_amps  > curr_pos_amps)  ? amp_slew  : -amp_slew;
  float step_v = (target_pos_volts > curr_pos_volts) ? volt_slew : -volt_slew;

  // Smoothly move watts servo
  if (abs(curr_pos_watts - target_pos_watts) > 1) {
    curr_pos_watts += step_w;
    curr_pos_watts = constrain(curr_pos_watts, 0, max_pos);
    watts_servo.write(curr_pos_watts);
  }

  // Smoothly move amps servo
  if (abs(curr_pos_amps - target_pos_amps) > 1) {
    curr_pos_amps += step_a;
    curr_pos_amps = constrain(curr_pos_amps, 0, max_pos);
    amps_servo.write(curr_pos_amps);
  }

  // Smoothly move volts servo (only if not near target)
  if (abs(curr_pos_volts - target_pos_volts) > 1) {
    curr_pos_volts += step_v;
    curr_pos_volts = constrain(curr_pos_volts, 0, max_pos);
    volts_servo.write(curr_pos_volts);
  }
}

// Adds small jitter to each servo that has stable input
void fluctuateNeedles() {
  static unsigned long last_update = 0;

  if (millis() - last_update >= 30) {
    last_update = millis();

    if (fluctuate_volts && random(0, 100) < 5) {
      float spike = random(-6, 7);  // spike between -8 and +8 degrees
      curr_pos_volts += spike;
      volts_servo.write(constrain(curr_pos_volts + spike, 0, max_pos));
    }

    if (fluctuate_amps && random(0, 100) < 5) {
      float spike = random(-6, 7);
      curr_pos_amps += spike;
      amps_servo.write(constrain(curr_pos_amps + spike, 0, max_pos));
    }

    if (fluctuate_watts && random(0, 100) < 3) {
      float spike = random(-3, 3);
      curr_pos_watts += spike;
      watts_servo.write(constrain(curr_pos_watts + spike, 0, max_pos));
    }
  }
}
