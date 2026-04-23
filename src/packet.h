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



#endif
