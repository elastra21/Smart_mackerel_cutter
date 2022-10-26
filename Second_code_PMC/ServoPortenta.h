#ifndef SERVO_PORTENTA_H
#define SERVO_PORTENTA_H
#include <Arduino.h>
#include <Arduino_MachineControl.h>

using namespace machinecontrol;

class ServoPortenta {
  public:
    ServoPortenta(uint8_t pin);
    void moveTo(unsigned int position);
  private:
    uint8_t servo;
};
#endif
