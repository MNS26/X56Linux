#ifndef USB_H
#define USB_H

#include <stdint.h>
#include <libusb-1.0/libusb.h>
#include "common.h"

#define VENDOR_ID        0x0738

#define INTERFACE_NUM    2

enum dev_type {
  DEV_NONE,
  DEV_THROTTLE,
  DEV_JOYSTICK,
};

#define MAX_DEVICES 8

struct x56_dev {
  libusb_device *libusb_dev;
  libusb_device_handle *handle;
  uint16_t vid;
  uint16_t pid;
  enum dev_type type;
  int id;
  int seen;
  uint8_t rgb_r, rgb_g, rgb_b;
  uint8_t bus;
  uint8_t addr;
  int active;
};

struct hotplug_ctx;

struct usb_ctx {
  libusb_context *ctx;
  struct x56_dev *devices[MAX_DEVICES];
  int device_count;
  struct hotplug_ctx *hotplug_ctx;
};

int send_control(struct x56_dev *dev, uint16_t w_value, uint8_t *data, uint16_t len);
int get_control(struct x56_dev *dev, uint16_t w_value, uint8_t *data, uint16_t len);
int read_interrupt(struct x56_dev *dev, uint8_t *data, size_t len);
struct usb_ctx *usb_init_ctx(void);
int usb_hotplug_init(struct usb_ctx *ctx, int (*callback)(enum dev_type, int, struct x56_dev *));
void usb_scan_devices(struct usb_ctx *ctx);
void usb_free_ctx(struct usb_ctx *ctx);
int usb_set_rgb(struct x56_dev *dev, uint8_t r, uint8_t g, uint8_t b);
int usb_save_rgb(struct x56_dev *dev);

#endif