#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>
#include <signal.h>
#include <syslog.h>

#include "usb.h"
#include "packet.h"

#define SOCKET_PATH "/run/x56-daemon.sock"

static volatile int running = 1;

static void signal_handler(int sig)
{
  (void)sig;
  running = 0;
}


static void handle_client(int client_fd, struct usb_ctx *ctx)
{
  packet_t pkt;
  memset(&pkt, 0, sizeof(packet_t));

  struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
  setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  int p_len = recv(client_fd, &pkt, sizeof(packet_t), 0);
  if (p_len != sizeof(packet_t)) {
    if (p_len < 0) perror("recv");
    return;
  }

  uint8_t received_crc = pkt.crc;
  pkt.crc = 0;
  uint8_t expected_crc = packet_crc(&pkt);
  if (received_crc != expected_crc) {
    packet_t err;
    memset(&err, 0, sizeof(packet_t));
    err.devices = 0;
    err.expecting_data = 0;
    err.last_packet = 0;
    err.w_value = 0;
    err.data[0] = 0;
    err.crc = packet_crc(&err);
    send(client_fd, &err, sizeof(packet_t), 0);
    return;
  }
  pkt.crc = received_crc;

  uint8_t first_expecting = pkt.expecting_data;

  int ret = 0;
  uint16_t w_value = pkt.w_value;
  uint8_t data[64];
  memcpy(data, pkt.data, 64);

  // When devices == 0, iterate through all slots and send to active devices
  
  fprintf(stdout, "received:\n");
  for (int i=0; i<64 ;i++) {
    fprintf(stdout, "%02X ",data[i]);
    if ((i+1)%8 == 0 )
      fprintf(stdout, "\n");
  }

  // Device 1 (index 0)
  if (pkt.devices == 0 || pkt.devices & 1) {
    struct x56_dev *d = ctx->devices[0];
    if (d && d->active) {
      ret = send_control(d, w_value, data, 64);
      fprintf(stdout, "ret=%d\n", ret);
      if (first_expecting && ret >= 0) {
        uint8_t response[64] = {0};
        read_interrupt(d, response, 64);
        memcpy(pkt.data, response, 64);
      }
    }
  }
  
  // Device 2 (index 1)
  if (pkt.devices == 0 || pkt.devices & 2) {
    struct x56_dev *d = ctx->devices[1];
    if (d && d->active) {
      ret = send_control(d, w_value, data, 64);
      if (first_expecting && ret >= 0) {
        uint8_t response[64] = {0};
        read_interrupt(d, response, 64);
        memcpy(pkt.data, response, 64);
      }
    }
  }
  
  // Device 3-8 (remaining slots)
  for (int i = 2; i < 8; i++) {
    if (pkt.devices == 0 || pkt.devices & (1 << i)) {
      struct x56_dev *d = ctx->devices[i];
      if (d && d->active) {
        ret = send_control(d, w_value, data, 64);
      }
    }
  }

  if (!first_expecting) {
    memset(&pkt, 0, sizeof(packet_t));
    pkt.devices = 0;
    pkt.expecting_data = first_expecting;
    pkt.last_packet = 0;
    pkt.w_value = 0;
    pkt.data[0] = ret >= 0 ? 1 : 0;
  }
  pkt.crc = packet_crc(&pkt);
  ssize_t sent = send(client_fd, &pkt, sizeof(packet_t), MSG_NOSIGNAL);
  if (sent < 0) {
    syslog(LOG_WARNING, "send failed: %s", strerror(errno));
  } else if (sent != sizeof(packet_t)) {
    syslog(LOG_WARNING, "send incomplete: %zd of %zu", sent, sizeof(packet_t));
  }
}

static int setup_socket(void)
{
  unlink(SOCKET_PATH);

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return -1;

  struct sockaddr_un addr = { .sun_family = AF_UNIX };
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }

  chmod(SOCKET_PATH, 0666);

  if (listen(fd, 5) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

static int device_callback(enum dev_type type, int connected)
{
  (void)type;
  (void)connected;
  return 0;
}

int main(int argc, char *argv[])
{
  (void)argc; (void)argv;

  openlog("x56d", LOG_PID, LOG_DAEMON);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  struct usb_ctx *ctx = usb_init_ctx();
  if (!ctx) {
    syslog(LOG_ERR, "Failed to init USB");
    return 1;
  }

  usb_scan_devices(ctx);

  if (usb_hotplug_init(ctx, device_callback) < 0) {
    syslog(LOG_WARNING, "Hotplug not available");
  }

  int sock_fd = setup_socket();
  if (sock_fd < 0) {
    syslog(LOG_ERR, "Failed to setup socket");
    usb_free_ctx(ctx);
    return 1;
  }

  syslog(LOG_INFO, "X-56 daemon running on %s", SOCKET_PATH);

  while (running) {
    struct pollfd fds[8];
    int nfds = 0;

    fds[nfds].fd = sock_fd;
    fds[nfds].events = POLLIN;
    nfds++;

    const struct libusb_pollfd **usb_polls = libusb_get_pollfds(ctx->ctx);
    if (usb_polls) {
      for (int i = 0; usb_polls[i] && nfds < 8; i++) {
        fds[nfds].fd = usb_polls[i]->fd;
        fds[nfds].events = POLLIN;
        nfds++;
      }
      free(usb_polls);
    }

    int ret = poll(fds, nfds, -1);
    if (ret < 0) {
      if (errno == EINTR) continue;
      break;
    }

    if (!running) break;

    for (int i = 0; i < nfds; i++) {
      if (fds[i].revents & POLLIN) {
        if (fds[i].fd == sock_fd) {
          int client_fd = accept(sock_fd, NULL, NULL);
          if (client_fd >= 0) {
            handle_client(client_fd, ctx);
            close(client_fd);
          }
        } else {
          struct timeval tv = {0, 10000};
          libusb_handle_events_timeout(ctx->ctx, &tv);
        }
      }
    }
  }

  close(sock_fd);
  unlink(SOCKET_PATH);
  usb_free_ctx(ctx);
  syslog(LOG_INFO, "X-56 daemon stopped");
  closelog();

  return 0;
}