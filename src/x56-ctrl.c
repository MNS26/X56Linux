#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <stdbool.h>

#include "common.h"
#include "packet.h"
#include "x56-ctrl.h"

#define SOCKET_PATH "/run/x56-daemon.sock"
#define MAX_DEVS 3

// this only works if we have the udev rules active
// will have to check in the code if this path exists or not
#define THROTTLE_PATH "/dev/x56-throttle"
#define JOYSTICK_PATH "/dev/x56-stick"

packet_t packet;

void printHelp() {
  fprintf(stderr, "Usage: x56-ctrl [-d ID] --get AXIS\n");
  fprintf(stderr, "       x56-ctrl [-d ID] -r R,G,B\n");
  fprintf(stderr, "  -d 1            Throttle\n");
  fprintf(stderr, "  -d 2            Joystick\n");
  fprintf(stderr, "  --get           Get configuration (requires -a for axis)\n");
  fprintf(stderr, "  -c|--calibrate Callibrate specific axis\n");
  fprintf(stderr, "  -a              Axis ID (30=X, 31=Y, 32=rotary1, etc.)\n");
  fprintf(stderr, "  -r|--rgb        RGB color (R,G,B)\n");
  fprintf(stderr, "  Example: x56-ctrl -d 1 -a 30 --get  (get throttle X config)\n");
  fprintf(stderr, "  Example: x56-ctrl -d 2 -r 255,0,0  (red joystick)\n");
}

void print_help_calibration() {
  fprintf(stderr, "Throttle: ");
  fprintf(stderr, "  ALL ");
  fprintf(stderr, "  THROTTLE1 ");
  fprintf(stderr, "  THROTTLE2 ");
  fprintf(stderr, "  ROTARY1 ");
  fprintf(stderr, "  ROTARY2 ");
  fprintf(stderr, "  ROTARY3 ");
  fprintf(stderr, "  ROTARY4\n");
  fprintf(stderr, "Joystick: ");
  fprintf(stderr, "  ALL ");
  fprintf(stderr, "  X ");
  fprintf(stderr, "  Y ");
  fprintf(stderr, "  Z\n");
  fprintf(stderr, "Examples:\n");
  fprintf(stderr, "  x56-ctrl -d 1 -c|-calibrate Throttle1\n");
  fprintf(stderr, "  x56-ctrl -d 2 -c|-calibrate X\n");

}

int main(int argc, char *argv[])
{

  uint8_t devs[MAX_DEVS] = {0};
  int dev_count = 0;
  bool need_end_packet = false;
  if (argc < 2) {
    printHelp();
    return 1;
  }

  int opt;
  int option_index = 0;
  uint8_t r, g, b;
  uint8_t get_config = 0;
  uint8_t axis = 0;
  uint8_t calibrate = 0;
  memset(&packet, 0, sizeof(packet_t));

  while ((opt = getopt_long(argc, argv, "hla:r:d:c:z:", long_options, &option_index)) !=-1) {
    switch (opt)
    {
    case 'h': // help
      printHelp();
      return 0;
      break;
    case 'd': // device
      packet.devices = 0;  // reset bitmask
      char *p = optarg;
      while (*p && dev_count < MAX_DEVS) {
        int dev_id = atoi(p);
        if (dev_id > 0 && dev_id <= 8) {
          packet.devices |= (1 << (dev_id - 1));  // set bit
        }
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
      }
      break;
    case 'a': // axis
      axis = atoi(optarg);
      break;
    case 'c': // calibrate
      // each packet in order
      // 0x0b 0x03 0x00 AXIS
      // 0x0b 0x04 0x00 AXIS
      // 0x0b 0x01 0x00 AXIS 0x00 0x00 0x00 0x00 0x03 0xe8 0x03 0xe8 0x00 0x00 0x01 0xf4 0x00 0x00
      // sample hid axis
      // 0x0b 0x03 0x00 AXIS
      // 0x0b 0x04 0x00 AXIS
      // 0x0b 0x03 0x00 AXIS
      // if we need to calibrate a specific axis. otherwise do whole device
      if (packet.devices == 0) {
        fprintf(stderr, "No device selected! Specify device before the calibrate option.\n");
        print_help_calibration();
        return 1;
      }
      if (argv[optind-1] == NULL) {
        fprintf(stderr, "No Axis speficied. The following are supported for each device.\n");
        print_help_calibration();
        return 1;
      }
      int axis_index=0;
      int axis_option = getopt_long_only(1, argv, "", axis_options,&axis_index);

//      set_axis_1(&packet,30);
//      send_packet(&packet);
//      set_axis_2(&packet,30);
//      send_packet(&packet);
//      calibrate_axis(
//        &packet,
//        1,
//        0,
//        30,
//        default_xsat,
//        default_ysat,
//        default_deadband,
//        default_curve,
//        default_profule,
//        0
//      );
//      send_packet(&packet);

if (packet.devices == 1)
      if (packet.devices == 2)
      if (packet.devices == 4)
      if (packet.devices == 8)
      if (packet.devices == 16)
      ;

      //sample axis here and average out

//      calibrate_axis(
//        &packet,
//        0,
//        1,
//        30,
//        default_xsat,
//        default_ysat,
//        default_deadband,
//        default_curve,
//        default_profule,
//        0 //measured axis value here
//      );
//      send_packet(&packet);
      return 0;
      break;
    case 'z':
      break;
    case 'r': // rgb
      if (sscanf(optarg, "%hhu,%hhu,%hhu", &r, &g, &b) != 3) {
        fprintf(stderr, "Invalid RGB format. Use: R,G,B (e.g., 255,0,0)\n");
        return 1;
      }
      set_rgb(&packet, r,g,b);
      send_packet(&packet);

      set_rgb_end(&packet);
      send_packet(&packet);
      break;
    case 4: // get
      get_config = 1;
      break;
    case 5: // curves
      break;
    case 6: // defaults
      break;
    
    default:
      break;
    }
  }


  return 0;
}

void set_configuration(packet_t *packet, configuration_t *configuration) {
}

configuration_t get_configuration(packet_t *packet)
{
  configuration_t configuration;
  memset(&configuration, 0, sizeof(configuration_t));
  configuration.axis = packet->data[17];
  configuration.xsat = (packet->data[13] << 8) | packet->data[12];
  configuration.ysat = (packet->data[15] << 8) | packet->data[14];
  configuration.deadband = (packet->data[11] << 8) | packet->data[10];
  configuration.curve = (packet->data[9] << 8) | packet->data[8];
  configuration.profile = packet->data[18];
  configuration.hall_call = (packet->data[20] << 8) | packet->data[19];
  return configuration;
}

void set_rgb(packet_t *packet, uint8_t r, uint8_t g, uint8_t b) {
  packet->expecting_data = 0;
  packet->last_packet = 1;
  packet->w_value = 0x0309;
  memset(packet->data, 0, 64);
  packet->data[0] = 0x09;
  packet->data[1] = 0x00;
  packet->data[2] = 0x03;
  packet->data[3] = r;
  packet->data[4] = g;
  packet->data[5] = b;
  packet->crc = packet_crc(packet);
}
void end_rgb(packet_t *packet) {
  packet->expecting_data = 0;
  packet->last_packet = 0;
  packet->w_value = 0x0300;
  memset(packet->data, 0, 64);
  packet->data[0] = 0x01;
  packet->data[1] = 0x01;
  packet->crc = packet_crc(packet);

}

void prepare_get_config(packet_t *packet, uint8_t axis) {
  packet->expecting_data = 1;
  packet->last_packet = 1;
  packet->w_value = 0x030b;
  memset(packet->data, 0, 64);
  packet->data[0] = 0x02;
  packet->data[1] = 0x00;
  packet->data[2] = 0x0b;
  packet->data[3] = axis;
  packet->crc = packet_crc(packet);
}

rgb_t get_rgb(packet_t *packet)
{
  rgb_t rgb;
  rgb.R = packet->data[3];
  rgb.G = packet->data[4];
  rgb.B = packet->data[5];
  return rgb;
}

void set_axis_1(packet_t *packet, uint8_t axis) {
  packet->expecting_data = 0;
  packet->last_packet = 0;
  packet->w_value = 0x030b;
  memset(packet->data, 0, 64);
  packet->data[0] = 0x0b;
  packet->data[1] = 0x03;
  packet->data[2] = 0x00;
  packet->data[3] = axis;
  packet->crc = packet_crc(packet);
}
void set_axis_2(packet_t *packet, uint8_t axis) {
  packet->expecting_data = 0;
  packet->last_packet = 0;
  packet->w_value = 0x030b;
  memset(packet->data, 0, 64);
  packet->data[0] = 0x0b;
  packet->data[1] = 0x04;
  packet->data[2] = 0x00;
  packet->data[3] = axis;
  packet->crc = packet_crc(packet);

}

void set_rgb_end(packet_t *packet) {
  packet->expecting_data = 0;
  packet->last_packet = 1;
  packet->w_value = 0x0300;
  memset(packet->data, 0, 64);
  packet->data[0] = 0x01;
  packet->data[1] = 0x01;
  packet->crc = packet_crc(packet);
}

void calibrate_axis(
                    packet_t *packet,
                    uint8_t set_defaults,
                    uint8_t is_last_packet,
                    uint8_t axis,
                    uint16_t Xsat,
                    uint16_t Ysat,
                    uint16_t deadband,
                    uint16_t curve,
                    uint8_t profile,
                    uint16_t axis_value) {
  packet->expecting_data = 0;
  packet->last_packet = is_last_packet;
  packet->w_value = 0x030b;
  memset(packet->data, 0, 64);
  packet->data[0] =  0x0b;
  packet->data[1] =  0x01;
  packet->data[2] =  0x00;
  packet->data[3] =  axis;
  packet->data[4] =  0x00;
  packet->data[5] =  set_defaults ? 0x00 : 0x01;
  packet->data[6] =  0x00;
  packet->data[7] =  set_defaults ? axis : 0x00;
  packet->data[8] =  set_defaults ? 0x03 : Xsat >> 4;         //XSAT
  packet->data[9] =  set_defaults ? 0xe8 : Xsat & 0xff;       //XSAT
  packet->data[10] = set_defaults ? 0x03 : Ysat >> 4;         //YSAT
  packet->data[11] = set_defaults ? 0xe8 : Ysat & 0xff;       //YSAT
  packet->data[12] = set_defaults ? 0x00 : deadband >> 4;     //DEADBAND
  packet->data[13] = set_defaults ? 0x00 : deadband & 0xff;   //DEADBAND
  packet->data[14] = set_defaults ? 0x01 : curve >> 4;        //CURVE
  packet->data[15] = set_defaults ? 0xf4 : curve & 0xff;      //CURVE
  packet->data[16] = set_defaults ? 0x00 : 0x00;              //PROFILE
  packet->data[17] = set_defaults ? 0x00 : axis_value >> 4;   //AXIS_VALUE
  packet->data[18] = set_defaults ? 0x00 : axis_value & 0xff; //AXIS_VALUE
  packet->crc = packet_crc(packet);
}

bool send_packet(packet_t *_packet) {
  packet_t packet = *_packet;
  // Setup socket file descriptor
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }

  // Set socket timeout
  struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));


  struct sockaddr_un addr = { .sun_family = AF_UNIX };
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

  // Open socket
  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    fprintf(stderr, "Error: Cannot connect to daemon. Is x56d running?\n");
    close(fd);
    return 1;
  }



//  if (get_config) {
//    if (axis == 0 && calibrate == 0) { // use axis id 0 with calibration to do all axis
//      fprintf(stderr, "Error: --get requires -a AXIS\n");
//      printHelp();
//      close(fd);
//      return 1;
//    }
//    prepare_get_config(&packet, axis);
//  }

  fprintf(stderr, "[CLIENT] sending packet: devices=0x%02X expecting=%d last=%d w_value=0x%04X\n",
    packet.devices,
    packet.expecting_data,
    packet.last_packet,
    packet.w_value
  );
  if (write(fd, &packet,sizeof(packet_t))!= sizeof(packet_t)) {
    perror("write");
    close(fd);
    return 1;
  }

//  if (need_end_packet && !get_config) {
//    packet_t end_pkt;
//    memset(&end_pkt, 0, sizeof(packet_t));
//    end_pkt.devices = packet->devices;
//    end_pkt.expecting_data = 0;
//    end_pkt.last_packet = 1;
//    end_pkt.w_value = 0x0300;
//    end_pkt.data[0] = 0x01;
//    end_pkt.data[1] = 0x01;
//    end_pkt.crc = packet_crc(&end_pkt);
//    fprintf(stderr, "[CLIENT] sending end packet: last=%d w_value=0x%04X\n", end_pkt.last_packet, end_pkt.w_value);
//    if (write(fd, &end_pkt,sizeof(packet_t))!= sizeof(packet_t)) {
//      perror("write");
//      close(fd);
//      return 1;
//    }
//  }

  if (read(fd, &packet,sizeof(packet_t))!=sizeof(packet_t)) {
    perror("read");
    close(fd);
    return 1;
  }
  
  // Verify response CRC
  uint8_t expected_crc = packet_crc(&packet);
  if (packet.crc != expected_crc) {
    fprintf(stderr, "CRC mismatch: got 0x%02X expected 0x%02X\n", packet.crc, expected_crc);
    close(fd);
    return 1;
  }
  close(fd);
  return 0;
}
