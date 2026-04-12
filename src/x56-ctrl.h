#ifndef X56_CTRL_H
#define X56_CTRL_H

#include <stdlib.h>
#include <stdint.h>
#include "packet.h"


// settings default
uint16_t default_xsat = 1000;
uint16_t default_ysat = 1000;
uint16_t default_curve = 1000;
uint16_t default_deadband = 1000;
uint8_t default_profule = 0;

typedef struct configuration {
  uint8_t axis;
  uint16_t xsat;
  uint16_t ysat;
  uint16_t curve;
  u_int16_t deadband;
  uint8_t profile;
  uint16_t hall_call;
} configuration_t; 

// Axis ID's
enum Joystick_id {
  X=30,
  Y=31,
  Z=32,
};
char* X_name = "X";
char* Y_name = "Y";
char* Z_name = "Z";

enum Throttle {
  throttle1 = 30,
  throttle2 = 31,
  rotary1   = 32,
  rotary2   = 35,
  rotary3   = 37,
  rotary4   = 36,
};

char* Throttle1_name = "X";
char* Throttle2_name = "X";
char* Rotary1_name = "Rotary1";
char* Rotary2_name = "Rotary2";
char* Rotary3_name = "Rotary3";
char* Rotary4_name = "Rotary4";


typedef struct __attribute__((packed)) RGB {
  uint8_t R;
  uint8_t G;
  uint8_t B;
} rgb_t;


void set_configuration(packet_t *packet, configuration_t *configuration);
configuration_t get_configuration(packet_t *packet);
void prepare_get_config(packet_t *packet, uint8_t axis);
void set_rgb(packet_t *packet, uint8_t r, uint8_t g, uint8_t b);
rgb_t get_rgb(packet_t *packet);

void set_end(packet_t *packet);

#endif //X56_CTRL_H