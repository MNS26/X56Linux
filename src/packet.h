#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stddef.h>

#define MAX_DEVICES 8

typedef enum {
    MSG_CMD,
    MSG_INPUT,
} msg_type_t;

typedef enum {
    CMD_NONE,
    CMD_GET,
    CMD_SET,
    CMD_CURVE,
    CMD_CALIBRATE,
    CMD_RESET,
    CMD_RGB,
    CMD_RGB_SAVE,
    CMD_START_INPUT,
    CMD_STOP_INPUT,
    CMD_LIST,
} cmd_type_t;



typedef struct {
    uint8_t device;
    msg_type_t msg_type;
    cmd_type_t cmd;
    uint8_t axis;
    uint8_t data[64];
} msg_t;

// Flags for partial axis config updates (stored in data[0])
#define CFG_XSAT            (1 << 0)
#define CFG_YSAT            (1 << 1)
#define CFG_DEADBAND        (1 << 2)
#define CFG_CURVE           (1 << 3)
#define CFG_PROFILE         (1 << 4)
#define CFG_CALIBRATION     (1 << 5)
#define CFG_GENERATE_CURVE  (1 << 7)  // Generate and store curve after config update

// Axis config values start at data[2] (16-bit aligned)
#define CFG_VALUE_XSAT      2
#define CFG_VALUE_YSAT      4
#define CFG_VALUE_DEADBAND  6
#define CFG_VALUE_CURVE     8
#define CFG_VALUE_PROFILE   10
#define CFG_VALUE_CALIBRATION 11

#endif
