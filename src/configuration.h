#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include <stdint.h>
#include <stddef.h>

// Settings for each axis
typedef struct configuration_t {
  uint16_t xsat;        // Horizontal graph axis (physical input)  (invert) 0 < 500 > 1000 (normal)  default = 1000
  uint16_t ysat;        // Vertical graph axis (transmitted input) (invert) 0 < 500 > 1000 (normal)  default = 1000
  uint16_t deadband;    // Deadzone range from center              (none  ) 0 < --- > 1000 (axis disabled) default = 0 
  uint16_t curve;       // Curvature of this axis                  (invert) 0 < 500 > 1000 (normal)  default = 500
  uint8_t profile;      // Profile mode for axis (not usefull for this)     0 = S, 1 = J             default = 0 
  uint16_t calibration; // Calibration value for hall effect axis (zero if not hall effect)
} configuration_t;

//typedef struct axis_configuration_t {
//  configuration_t config;
//} axis_configuration_t;

// Curve of each axis

typedef struct axis_configuration_t {
  configuration_t config;
  uint8_t curve[1<<8]
} axis_configuration_8b_t;
typedef struct axis_configuration_t {
  configuration_t config;
  uint16_t curve[1<<10]
} axis_configuration_10b_t;
typedef struct axis_configuration_t {
  configuration_t config;
  uint16_t curve[1<<12]
} axis_configuration_12b_t;
typedef struct axis_configuration_t {
  configuration_t config;
  uint16_t curve[1<<16]
} axis_configuration_16b_t;

// Throttle config struct
typedef struct configuration_throttle_t {
// possible "hidden axis" thumb stick X and Y
  axis_configuration_10b_t Throttle1;
  axis_configuration_10b_t Throttle2;
  axis_configuration_8b_t Rotary1;
  axis_configuration_8b_t Rotary2;
  axis_configuration_8b_t Rotary3;
  axis_configuration_8b_t Rotary4;
} configuration_throttle_t;

// Joystick config struct
typedef struct configuration_joystick_t {
// possible "hidden axis" thumb stick X and Y
  axis_configuration_16b_t X;
  axis_configuration_16b_t Y;
  axis_configuration_10b_t Rz;
} configuration_joystick_t;



#endif //CONFIGURATION_H