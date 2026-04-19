#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "usb.h"

struct hotplug_ctx {
  struct usb_ctx *ctx;
  int (*callback)(enum dev_type type, int action);
  libusb_hotplug_callback_handle callback_handle;
};

int send_control(struct x56_dev *dev, uint16_t w_value, uint8_t *data, uint16_t len)
{
  return libusb_control_transfer(
    dev->handle,
    0x21,
    0x09,
    w_value,
    INTERFACE_NUM,
    data,
    len,
    1000
  );
}

int get_control(struct x56_dev *dev, uint16_t w_value, uint8_t *data, uint16_t len)
{
  return libusb_control_transfer(
    dev->handle,
    0xA1,
    0x01,
    w_value,
    INTERFACE_NUM,
    data,
    len,
    1000
  );
}

int read_interrupt(struct x56_dev *dev, uint8_t *data, size_t len)
{
  return libusb_interrupt_transfer(
    dev->handle,
    0x82,
    data,
    len,
    NULL,
    1000
  );
}

static int claim_interface(struct x56_dev *dev)
{
  int ret = libusb_detach_kernel_driver(dev->handle, INTERFACE_NUM);
  if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND && ret != LIBUSB_ERROR_NO_DEVICE) {
    fprintf(stderr, "detach_kernel_driver failed: %d\n", ret);
  }

  ret = libusb_claim_interface(dev->handle, INTERFACE_NUM);
  if (ret < 0) {
    fprintf(stderr, "claim_interface failed: %d\n", ret);
    return -1;
  }
  fprintf(stderr, "Interface claimed successfully\n");
  return 0;
}

static int handle_hotplug(libusb_context *ctx, libusb_device *device,
              libusb_hotplug_event event, void *user_data)
{
  (void)ctx;
  struct hotplug_ctx *hp = user_data;
  struct usb_ctx *uctx = hp->ctx;

  struct libusb_device_descriptor desc;
  if (libusb_get_device_descriptor(device, &desc) != 0) return 0;

  if (desc.idVendor != VENDOR_ID) return 0;

  uint8_t bus = libusb_get_bus_number(device);
  uint8_t addr = libusb_get_device_address(device);

  if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
    enum dev_type type = 0;
    const char *name = NULL;

    if (desc.idProduct == PRODUCT_THROTTLE) {
      type = DEV_THROTTLE;
      name = "X-56 Throttle";
    } else if (desc.idProduct == PRODUCT_JOYSTICK) {
      type = DEV_JOYSTICK;
      name = "X-56 Joystick";
    }

    if (!type) return 0;

    struct x56_dev *dev = calloc(1, sizeof(*dev));
    if (!dev) return 0;

    dev->type = type;
    dev->bus = bus;
    dev->addr = addr;
    dev->id = type == DEV_THROTTLE ? 1 : 2;

    if (libusb_open(device, &dev->handle) != 0) {
      fprintf(stderr, "Failed to open device %s\n", name);
      free(dev);
      return 0;
    }

    if (claim_interface(dev) < 0) {
      fprintf(stderr, "Failed to claim interface for %s (id=%d)\n", name, dev->id);
      libusb_close(dev->handle);
      free(dev);
      return 0;
    }

    dev->active = 1;
    uctx->devices[dev->id - 1] = dev;

    fprintf(stderr, "Device connected: %s (id=%d, bus %u device %u)\n",
        name, dev->id, bus, addr);

    if (hp->callback) {
      hp->callback(type, 1);
    }

  } else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
    for (int i = 0; i < MAX_DEVICES; i++) {
      struct x56_dev *d = uctx->devices[i];
      if (d && d->bus == bus && d->addr == addr) {
        enum dev_type dev_type = d->type;
        fprintf(stderr, "Device disconnected: id=%d\n", d->id);
        if (d->handle) libusb_close(d->handle);
        free(d);
        uctx->devices[i] = NULL;
        if (hp->callback) {
          hp->callback(dev_type, 0);
        }
        break;
      }
    }
  }

  return 0;
}

struct usb_ctx *usb_init_ctx(void)
{
  struct usb_ctx *ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    return NULL;
  }

  if (libusb_init(&ctx->ctx) != 0) {
    free(ctx);
    return NULL;
  }

  ctx->device_count = 0;
  libusb_set_option(ctx->ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_ERROR);
  return ctx;
}

int usb_hotplug_init(struct usb_ctx *ctx, int (*callback)(enum dev_type, int))
{
  struct hotplug_ctx *hp = calloc(1, sizeof(*hp));
  if (!hp) return -1;

  hp->ctx = ctx;
  hp->callback = callback;

  int ret = libusb_hotplug_register_callback(ctx->ctx,
    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
    LIBUSB_HOTPLUG_NO_FLAGS,
    VENDOR_ID,
    LIBUSB_HOTPLUG_MATCH_ANY,
    LIBUSB_HOTPLUG_MATCH_ANY,
    handle_hotplug,
    hp,
    &hp->callback_handle);

  if (ret != LIBUSB_SUCCESS) {
    free(hp);
    return -1;
  }

  ctx->hotplug_ctx = hp;
  return 0;
}

void usb_scan_devices(struct usb_ctx *ctx)
{
  if (!ctx || !ctx->ctx) return;

  libusb_device **list;
  ssize_t cnt = libusb_get_device_list(ctx->ctx, &list);
  if (cnt < 0) return;

  for (ssize_t i = 0; i < cnt; i++) {
    libusb_device *d = list[i];
    struct libusb_device_descriptor desc;
    
    if (libusb_get_device_descriptor(d, &desc) != 0) continue;
    if (desc.idVendor != VENDOR_ID) continue;
    
    uint8_t bus = libusb_get_bus_number(d);
    uint8_t addr = libusb_get_device_address(d);
    
    enum dev_type type = 0;
    const char *name = NULL;
    
    if (desc.idProduct == PRODUCT_THROTTLE) {
      type = DEV_THROTTLE;
      name = "X-56 Throttle";
    } else if (desc.idProduct == PRODUCT_JOYSTICK) {
      type = DEV_JOYSTICK;
      name = "X-56 Joystick";
    }
    
    if (!type) continue;
    
    struct x56_dev *dev = calloc(1, sizeof(*dev));
    if (!dev) continue;
    
    dev->type = type;
    dev->bus = bus;
    dev->addr = addr;
    dev->id = type == DEV_THROTTLE ? 1 : 2;
    
    if (libusb_open(d, &dev->handle) != 0) {
      free(dev);
      continue;
    }
    
    if (claim_interface(dev) < 0) {
      fprintf(stderr, "Failed to claim interface for %s (id=%d)\n", name, dev->id);
      libusb_close(dev->handle);
      free(dev);
      continue;
    }
    
    dev->active = 1;
    ctx->devices[dev->id - 1] = dev;
    ctx->device_count++;
    
    fprintf(stderr, "Device found: %s (id=%d, bus %u device %u)\n",
        name, dev->id, bus, addr);
  }

  libusb_free_device_list(list, 0);
}

void usb_free_ctx(struct usb_ctx *ctx)
{
  if (!ctx) return;

  if (ctx->hotplug_ctx) {
    libusb_hotplug_deregister_callback(ctx->ctx, ctx->hotplug_ctx->callback_handle);
    free(ctx->hotplug_ctx);
  }

  for (int i = 0; i < MAX_DEVICES; i++) {
    struct x56_dev *d = ctx->devices[i];
    if (d) {
      if (d->handle) {
        libusb_close(d->handle);
      }
      free(d);
    }
  }

  if (ctx->ctx) libusb_exit(ctx->ctx);
  free(ctx);
}

int usb_set_rgb(struct x56_dev *dev, uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t endPacket[64] = {0x01, 0x01};
  uint8_t packet[64] = {0};
  packet[0] = 0x09;
  packet[1] = 0x00;
  packet[2] = 0x03;
  packet[3] = r;
  packet[4] = g;
  packet[5] = b;

  int ret = send_control(dev, 0x0309, packet, 64);
  ret = send_control(dev, 0x0300, endPacket, 64);
  if (ret < 0) {
    return ret;
  }

  return 0;
}