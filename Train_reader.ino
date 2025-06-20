#include <Servo.h>

Servo watts_servo;  // create Servo object to control a servo
Servo amps_servo;
Servo volts_servo;

int max_pos = 170;
int curr_pos_volts;
int curr_pos_amps;
int curr_pos_watts;

void setup() {
  watts_servo.attach(9);  // attaches the servo on pin 9 to the Servo object
  amps_servo.attach(10);
  volts_servo.attach(11);
  
  watts_servo.write(max_pos);
  amps_servo.write(max_pos);
  volts_servo.write(max_pos);
  curr_pos_volts = max_pos;
  curr_pos_amps = max_pos;
  curr_pos_watts = max_pos;
  
  Serial.begin(9600);
  Serial.println("Enter: volts amps (e.g. 5 2)");
}

void loop() {
  if (Serial.available()) {
    int voltage = Serial.parseInt();
    int amps  = Serial.parseInt();
    int watts = voltage * amps;
    int volts_pos = max_pos - (voltage / 20.0) * max_pos;
    int amps_pos = max_pos - (amps / 10.0) * max_pos;
    int watts_pos = max_pos - (watts / 14.5) * max_pos;

    // Wait for full input to be received
    if (Serial.read() == '\n') {
      // Clamp to valid servo range
      watts_pos = constrain(watts_pos, 0, max_pos);
      amps_pos  = constrain(amps_pos, 0, max_pos);
      volts_pos = constrain(volts_pos, 0, max_pos);
    
      moveServos(watts_pos, amps_pos, volts_pos);
    }
  }
}

void moveServos(int target_pos_watts, int target_pos_amps, int target_pos_volts) {
  int stepW = (target_pos_watts > curr_pos_watts) ? 1 : -1;
  int stepA = (target_pos_amps > curr_pos_amps) ? 1 : -1;
  int stepV = (target_pos_volts > curr_pos_volts) ? 1 : -1;

  while (curr_pos_watts != target_pos_watts || curr_pos_amps != target_pos_amps || curr_pos_volts != target_pos_volts) {
    if (curr_pos_watts != target_pos_watts) {
      curr_pos_watts += stepW;
      watts_servo.write(curr_pos_watts);
    }

    if (curr_pos_amps != target_pos_amps) {
      curr_pos_amps += stepA;
      amps_servo.write(curr_pos_amps);
    }

    if (curr_pos_volts != target_pos_volts) {
      curr_pos_volts += stepV;
      volts_servo.write(curr_pos_volts);
    }

    delay(20);
  }
}
