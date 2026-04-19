#ifndef COMMON_H
#define COMMON_H
#define PRODUCT_JOYSTICK 0x2221
#define PRODUCT_THROTTLE 0xa221
#define VENDOR 0x0738
#include <stdlib.h>
#include <stdint.h>

typedef struct x56_joytick_report_t {
  uint16_t X;
  uint16_t Y;
  uint16_t Rz:12;
  uint8_t Hat1:4;
  uint8_t btn1:1;
  uint8_t btn2:1;
  uint8_t btn3:1;
  uint8_t btn4:1;
  uint8_t btn5:1;
  uint8_t btn6:1;
  uint8_t btn7:1;
  uint8_t btn8:1;
  uint8_t btn9:1;
  uint8_t btn10:1;
  uint8_t btn11:1;
  uint8_t btn12:1;
  uint8_t btn13:1;
  uint8_t btn14:1;
  uint8_t btn15:1;
  uint8_t btn16:1;
  uint8_t btn17:1;
  uint8_t Rx;
  uint8_t Ry;
} x56_joytick_report_t;

typedef struct x56_throttle_report_t {
  uint16_t X:10;
  uint16_t Y:10;
  uint8_t btn1:1;
  uint8_t btn2:1;
  uint8_t btn3:1;
  uint8_t btn4:1;
  uint8_t btn5:1;
  uint8_t btn6:1;
  uint8_t btn7:1;
  uint8_t btn8:1;
  uint8_t btn9:1;
  uint8_t btn10:1;
  uint8_t btn11:1;
  uint8_t btn12:1;
  uint8_t btn13:1;
  uint8_t btn14:1;
  uint8_t btn15:1;
  uint8_t btn16:1;
  uint8_t btn17:1;
  uint8_t btn18:1;
  uint8_t btn19:1;
  uint8_t btn20:1;
  uint8_t btn21:1;
  uint8_t btn22:1;
  uint8_t btn23:1;
  uint8_t btn24:1;
  uint8_t btn25:1;
  uint8_t btn26:1;
  uint8_t btn27:1;
  uint8_t btn28:1;
  uint8_t btn29:1;
  uint8_t btn30:1;
  uint8_t btn31:1;
  uint8_t btn32:1;
  uint8_t btn33:1;
  uint8_t btn34:1;
  uint8_t btn35:1;
  uint8_t btn36:1;
  uint8_t Z;
  uint8_t Rx;
  uint8_t Rz;
  uint8_t Ry;
  uint8_t Slider;
  uint8_t Dial;
} x56_throttle_report_t;

typedef struct axis_config_t {
  uint16_t x_sat;
  uint16_t y_sat;
  uint16_t deadband;
  uint16_t curve;
  uint8_t profile;
  uint16_t hall_calibration; // zero for non hall effect axis
} axis_config_t;



#endif //COMMON_H