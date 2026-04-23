#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include <stdint.h>
#include <stddef.h>

// Settings for each axis
typedef struct configuration_t{
  uint16_t xsat;
  uint16_t ysat;
  uint16_t deadband;
  uint16_t curve;
  uint8_t profile;
  uint16_t calibration;
} configuration_t;

// Curve of each axis
typedef struct axis_configuration_t{
  configuration_t config;
} axis_configuration_t;

// Throttle config struct
typedef struct configuration_throttle_t{
// possible "hidden axis" thumb stick X and Y
  axis_configuration_t Throttle1;
  axis_configuration_t Throttle2;
  axis_configuration_t Rotary1;
  axis_configuration_t Rotary2;
  axis_configuration_t Rotary3;
  axis_configuration_t Rotary4;
} configuration_throttle_t;

// Joystick config struct
typedef struct configuration_joystick_t{
// possible "hidden axis" thumb stick X and Y
  axis_configuration_t X;
  axis_configuration_t Y;
  axis_configuration_t Rz;
} configuration_joystick_t;



#endif //CONFIGURATION_H