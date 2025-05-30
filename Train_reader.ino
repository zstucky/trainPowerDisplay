#include <Servo.h>

Servo myservo_watts;  // create Servo object to control a servo
Servo myservo_amps;
Servo myservo_volts;
// twelve Servo objects can be created on most boards

int pos = 170;    // variable to store the servo position

void setup() {
  myservo_watts.attach(9);  // attaches the servo on pin 9 to the Servo object
  myservo_amps.attach(10);
  myservo_volts.attach(11);
  
  myservo_watts.write(pos);
  myservo_amps.write(pos);
  myservo_volts.write(pos);
  
}

void loop() {

}
