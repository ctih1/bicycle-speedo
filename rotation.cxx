
#include <Arduino.h>
#include <deque>

#define INTERNAL_LED 2
#define HALLEFFECT 27
#define SLEEP_TIME 10

using std::to_string;
using std::deque;

std::deque<bool> rotation_times = {};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(INTERNAL_LED, OUTPUT);
  pinMode(HALLEFFECT, INPUT_PULLUP);
  Serial.println("Setup");
}

// Testing stuff for hall effect sensors
void loop() {
  int state = digitalRead(HALLEFFECT);

  if(state == LOW) {
    digitalWrite(INTERNAL_LED, HIGH);  // Turn the LED ON (or use 255 for brightness)
  } else {
    digitalWrite(INTERNAL_LED, LOW);   // Turn the LED OFF (or use 0 for brightness)
  }

  rotation_times.push_front(state == LOW);
  if(rotation_times.size() > 3000/SLEEP_TIME) {
    rotation_times.pop_back();
  }

  bool last_val = false;
  int rotation_last_second = 0;

  for(bool val : rotation_times) {
    if(last_val && val) continue;

    if(val) {
      rotation_last_second += 1;
    }

    last_val = val;
  }

  Serial.printf("RPM: %f\n", ((double)rotation_last_second/3.0)*60);
  
  delay(SLEEP_TIME);
}

