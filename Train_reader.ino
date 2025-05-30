#include <Servo.h>

Servo myservo_watts;  // create Servo object to control a servo
Servo myservo_amps;
Servo myservo_volts;
// twelve Servo objects can be created on most boards

void setup() {
  myservo_watts.attach(9);  // attaches the servo on pin 9 to the Servo object
  myservo_amps.attach(10);
  myservo_volts.attach(11);
  
  myservo_watts.write(170);
  myservo_amps.write(170);
  myservo_volts.write(170);
  
  Serial.begin(9600);
  Serial.println("Enter: watts amps volts (e.g. 90 45 135)");
}

void loop() {
  if (Serial.available()) {
    int watts_pos = Serial.parseInt();
    int amps_pos  = Serial.parseInt();
    int volts_pos = Serial.parseInt();

    // Wait for full input to be received
    if (Serial.read() == '\n') {
      // Clamp to valid servo range
      watts_pos = constrain(watts_pos, 0, 170);
      amps_pos  = constrain(amps_pos, 0, 170);
      volts_pos = constrain(volts_pos, 0, 170);

      myservo_watts.write(watts_pos);
      myservo_amps.write(amps_pos);
      myservo_volts.write(volts_pos);
    }
  }
}
