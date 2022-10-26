#include "ServoPortenta.h"

ServoPortenta::ServoPortenta(uint8_t pin){
  servo = pin;
}

void ServoPortenta::moveTo(unsigned int position){
  const unsigned int val = (position*10.25)+500;
  digital_programmables.set(servo, HIGH);
  delayMicroseconds(val);
  digital_programmables.set(servo, LOW);
}