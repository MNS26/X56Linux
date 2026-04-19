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
  // thumb1? (32) 
  // thumn2? (33)
  // ???     (34)
  Z=35,
};

enum Throttle {
  throttle1 = 30,
  throttle2 = 31,
  rotary1   = 32,
  // thumb1? (missing 33)
  // thumb2? (missing 34)
  rotary2   = 35,
  rotary4   = 36,
  rotary3   = 37,
};

char* Throttle1_name = "Throttle1";
char* Throttle2_name = "Throttle2";
char* Rotary1_name = "Rotary1";
char* Rotary2_name = "Rotary2";
char* Rotary3_name = "Rotary3";
char* Rotary4_name = "Rotary4";


typedef struct __attribute__((packed)) RGB {
  uint8_t R;
  uint8_t G;
  uint8_t B;
} rgb_t;

static struct option long_options[] = {
  {"help", no_argument, 0, 'h'},
  {"device", required_argument, 0, 'd'},
  {"deadzone", required_argument, 0, 'z'},
  {"axis", required_argument, 0, 'a'},
  {"rgb", required_argument, 0, 'r'},
  {"calibrate", optional_argument, 0, 'c'},
  {"get", no_argument, 0, 4},
  {"curve", required_argument, 0, 5},
  {"defaults", no_argument, 0, 7},
  {0, 0, 0, 0},
};

static struct option axis_options[] = {
  {"x",         no_argument, 0, 30},
  {"y",         no_argument, 0, 31},
  // missing 32 (joystick)
  // missing 33 (joystick)
  // missing 34 (joystick)
  {"z",         no_argument, 0, 35},
  {"throttle1", no_argument, 0, 30},
  {"throttle2", no_argument, 0, 31},
  {"rotary1",   no_argument, 0, 32},
  // missing 33 (throttle)
  // missing 34 (throttle)
  {"rotary2",   no_argument, 0, 35},
  {"rotary4",   no_argument, 0, 36},
  {"rotary3",   no_argument, 0, 37},
  {0, 0, 0, 0},
};

void set_configuration(packet_t *packet, configuration_t *configuration);
configuration_t get_configuration(packet_t *packet);
void prepare_get_config(packet_t *packet, uint8_t axis);

void set_axis_1(packet_t *packet, uint8_t axis);
void set_axis_2(packet_t *packet, uint8_t axis);
void set_rgb(packet_t *packet, uint8_t r, uint8_t g, uint8_t b);
rgb_t get_rgb(packet_t *packet);
void set_rgb_end(packet_t *packet);

void calibrate_axis(packet_t *packet, uint8_t set_defaults, uint8_t is_last_packet, uint8_t axis, uint16_t Xsat, uint16_t Ysat, uint16_t deadband, uint16_t curve, uint8_t profile, uint16_t axis_value);
bool send_packet(packet_t *_packet);

#endif //X56_CTRL_H