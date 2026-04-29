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
#include <signal.h>
#include <errno.h>

#include "common.h"
#include "packet.h"
#include "usb.h"

#define SOCKET_PATH "/tmp/x56-daemon.sock"
#define MAX_DEVS 8

static volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static uint8_t get_axis_id(const char *name) {
    if (!name || name[0] == 0) return 0;

    
    if (
        strcasecmp(name, "X")         == 0 ||
        strcasecmp(name, "Throttle1") == 0 ||
        strcasecmp(name, "T1")        == 0
    ) return 0x30;
    if (
        strcasecmp(name, "Y")         == 0 ||
        strcasecmp(name, "Throttle2") == 0 ||
        strcasecmp(name, "T2")        == 0
    ) return 0x31;
    if (
        strcasecmp(name, "Rotary1")   == 0 ||
        strcasecmp(name, "R1")        == 0
    ) return 0x32;
    if (
        strcasecmp(name, "Z")         == 0 ||
        strcasecmp(name, "Rotary2")   == 0 ||
        strcasecmp(name, "R2")        == 0
    ) return 0x35;
    if (
        strcasecmp(name, "Rotary4")   == 0 ||
        strcasecmp(name, "R4")        == 0
    ) return 0x36;
    if (
        strcasecmp(name, "Rotary3")   == 0 ||
        strcasecmp(name, "R3")        == 0
    ) return 0x37;
//    if (strcasecmp(name, "Unknown1" ) == 0 || strcasecmp(name, "U1" ) == 0) return 0x33;
//    if (strcasecmp(name, "Unknown2" ) == 0 || strcasecmp(name, "U2" ) == 0) return 0x34;

    return (uint8_t)atoi(name);
}

void printHelp() {
    fprintf(stderr, "Usage: x56-ctrl -d ID [COMMAND] [OPTIONS]\n");
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  -l|--list        List connected devices\n");
    fprintf(stderr, "  -g|--get AXIS    Get axis configuration\n");
    fprintf(stderr, "  -s|--set AXIS    Set axis configuration\n");
    fprintf(stderr, "  -c|--calibrate   Calibrate axis (rest position)\n");
    fprintf(stderr, "  -R|--reset       Reset axis to defaults\n");
    fprintf(stderr, "  -u|--upload      Upload stored curve to device\n");
    fprintf(stderr, "  -r|--rgb R,G,B   Set RGB LED color\n");
    fprintf(stderr, "  -S|--save        Save RGB color on the device\n");
    fprintf(stderr, "  -i|--input       Start input stream\n");
    fprintf(stderr, "  -q|--stop        Stop input stream\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -d|--device ID   Device (1 or 2, 0 for all)\n");
    fprintf(stderr, "  -x|--xsat N      X saturation\n");
    fprintf(stderr, "  -y|--ysat N      Y saturation\n");
    fprintf(stderr, "  -z|--deadzone N  Deadzone\n");
    fprintf(stderr, "  -k|--curve N     Curve\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  x56-ctrl -l                         List devices\n");
    fprintf(stderr, "  x56-ctrl -d 1 -g X                Get X axis config\n");
    fprintf(stderr, "  x56-ctrl -d 1 -s X -z 500       Set deadzone\n");
    fprintf(stderr, "  x56-ctrl -d 1 -s X -x 1000 -y 1000 -z 500 -k 500\n");
    fprintf(stderr, "  x56-ctrl -d 2 -c X              Calibrate X axis\n");
    fprintf(stderr, "  x56-ctrl -d 0 -r 255,0,0    Red LED on all devices\n");
    fprintf(stderr, "  x56-ctrl -d 0 -S     Save LED colors on all devices\n");
}

static struct option long_options[] = {
    {"device",      required_argument,  0, 'd'},
    {"list",        no_argument,        0, 'l'},
    {"get",         optional_argument,  0, 'g'},
    {"set",         required_argument,  0, 's'},
    {"calibrate",   optional_argument,  0, 'c'},
    {"reset",       optional_argument,  0, 'R'},
    {"upload",      optional_argument,  0, 'u'},
    {"rgb",         required_argument,  0, 'r'},
    {"save",        no_argument,        0, 'S'},
    {"input",       no_argument,        0, 'i'},
    {"stop",        no_argument,        0, 'q'},
    {"xsat",        required_argument,  0, 'x'},
    {"ysat",        required_argument,  0, 'y'},
    {"deadzone",    required_argument,  0, 'z'},
    {"curve",       required_argument,  0, 'k'},
    {"help",        no_argument,        0, 'h'},
    {0, 0, 0, 0},
};

static int connect_daemon(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error: Cannot connect to daemon. Is x56d running?\n");
        close(fd);
        return -1;
    }

    return fd;
}

static int send_cmd(uint8_t device, cmd_type_t cmd, uint8_t axis, uint8_t *data, size_t data_len) {
    int fd = connect_daemon();
    if (fd < 0) return -1;

    msg_t msg = {0};
    msg.device = device;
    msg.msg_type = MSG_CMD;
    msg.cmd = cmd;
    msg.axis = axis;
    if (data && data_len > 0) {
        memcpy(msg.data, data, data_len < 64 ? data_len : 64);
    }

    if (write(fd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
        perror("write");
        close(fd);
        return -1;
    }

    if (cmd == CMD_START_INPUT || cmd == CMD_CALIBRATE) {
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        while (running) {
            ssize_t n = read(fd, &msg, sizeof(msg_t));
            if (n <= 0) {
                if (errno == EINTR) continue;
                break;
            }

            if (msg.msg_type == MSG_INPUT) {
                printf("%02X%02X%02X%02X%02X%02X%02X%02X\n",
                    msg.data[0], msg.data[1], msg.data[2], msg.data[3],
                    msg.data[4], msg.data[5], msg.data[6], msg.data[7]);
                fflush(stdout);
            } else if (msg.msg_type == MSG_CMD && msg.cmd == CMD_NONE) {
                if (msg.data[0] == 0) {
                    fprintf(stderr, "Success\n");
                } else {
                    fprintf(stderr, "Failed: %d\n", msg.data[0]);
                }
                break;
            }
        }
    } else {
        while (1) {
            ssize_t n = read(fd, &msg, sizeof(msg_t));
            if (n != sizeof(msg_t)) {
                break;
            }

            if (msg.msg_type == MSG_CMD && msg.cmd == CMD_GET) {
                fprintf(stderr, "Device %d:\n", msg.device);
                axis_config_t *cfg = (axis_config_t*)msg.data;
                printf("  axis: %d\n", msg.axis);
                printf("  xsat: %d\n", cfg->x_sat);
                printf("  ysat: %d\n", cfg->y_sat);
                printf("  deadzone: %d\n", cfg->deadband);
                printf("  curve: %d\n", cfg->curve);
                printf("  profile: %d\n", cfg->profile);
                printf("  calibration: %d\n", cfg->hall_calibration);
            } else if (msg.msg_type == MSG_CMD && msg.cmd == CMD_LIST) {
                const char *name = msg.data[0] == DEV_JOYSTICK ? "Joystick" :
                                 msg.data[0] == DEV_THROTTLE ? "Throttle" : "Unknown";
                printf("Device %d: %s (active: %s)\n", msg.device, name, msg.data[1] ? "yes" : "no");
            } else if (msg.msg_type == MSG_CMD && msg.cmd == CMD_CALIBRATE) {
                axis_config_t *cfg = (axis_config_t*)msg.data;
                printf("calibration complete: %d\n", cfg->hall_calibration);
            } else if (msg.msg_type == MSG_CMD && msg.cmd == CMD_NONE) {
                if (msg.data[0] == 0) {
                    fprintf(stderr, "OK\n");
                } else {
                    fprintf(stderr, "Error: %d\n", msg.data[0]);
                }
                break;
            } else {
                break;
            }
        }
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    uint8_t device = 0;
    cmd_type_t cmd = CMD_NONE;
    uint8_t axis = 0;
    uint8_t set_data[64] = {0};
    int set_count = 0;

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "d:lg:s:c:R:r:iSqx:y:z:k:u:h",
        long_options, &option_index)) != -1) {
        switch (opt) {
        case 'd':
            device = atoi(optarg);
            if (device > 2) {
                fprintf(stderr, "Invalid device: %s\n", optarg);
                return 1;
            }
            break;
        case 'l':
            cmd = CMD_LIST;
            break;
        case 'g':
            cmd = CMD_GET;
            axis = get_axis_id(optarg);
            break;
        case 's':
            cmd = CMD_SET;
            if (optarg) {
                axis = get_axis_id(optarg);
            }
            break;
        case 'c':
            cmd = CMD_CALIBRATE;
            if (optarg) {
                axis = get_axis_id(optarg);
            }
            break;
        case 'R':
            cmd = CMD_RESET;
            if (optarg) {
                axis = get_axis_id(optarg);
            }
            break;
        case 'u':
            cmd = CMD_CURVE;
            if (optarg) {
                axis = get_axis_id(optarg);
            } else {
                fprintf(stderr, "-u requires an axis argument (e.g., -u Throttle1)\n");
                return 1;
            }
            break;
        case 'r': {
            if (!optarg) {
                fprintf(stderr, "RGB requires value (e.g., -r 255,0,0)\n");
                return 1;
            }
            cmd = CMD_RGB;
            uint8_t r, g, b;
            if (sscanf(optarg, "%hhu,%hhu,%hhu", &r, &g, &b) != 3) {
                fprintf(stderr, "Invalid RGB: %s\n", optarg);
                return 1;
            }
            set_data[0] = r;
            set_data[1] = g;
            set_data[2] = b;
            set_count = 3;
            break;
        }
        case 'S':
            cmd = CMD_RGB_SAVE;
            break;
        case 'i':
            cmd = CMD_START_INPUT;
            break;
        case 'q':
            cmd = CMD_STOP_INPUT;
            break;
        case 'x':
            if (cmd != CMD_SET) {
                fprintf(stderr, "-x requires -s|--set\n");
                return 1;
            }
            set_data[0] |= CFG_XSAT;
            *(uint16_t*)&set_data[CFG_VALUE_XSAT] = atoi(optarg);
            break;
        case 'y':
            if (cmd != CMD_SET) {
                fprintf(stderr, "-y requires -s|--set\n");
                return 1;
            }
            set_data[0] |= CFG_YSAT;
            *(uint16_t*)&set_data[CFG_VALUE_YSAT] = atoi(optarg);
            break;
        case 'z':
            if (cmd != CMD_SET) {
                fprintf(stderr, "-z requires -s|--set\n");
                return 1;
            }
            set_data[0] |= CFG_DEADBAND;
            *(uint16_t*)&set_data[CFG_VALUE_DEADBAND] = atoi(optarg);
            break;
        case 'k':
            if (cmd != CMD_SET) {
                fprintf(stderr, "-k requires -s|--set\n");
                return 1;
            }
            set_data[0] |= CFG_CURVE;
            *(uint16_t*)&set_data[CFG_VALUE_CURVE] = atoi(optarg);
            break;
        case 'h':
        default:
            printHelp();
            return (opt == 'h') ? 0 : 1;
        }
    }

    if (device < 0 || device > 2) {
        fprintf(stderr, "Device required (-d 1 or -d 2)\n");
        return 1;
    }

    if (cmd == CMD_NONE) {
        fprintf(stderr, "Command required\n");
        return 1;
    }

    // For SET commands, ensure we send enough data
    if (cmd == CMD_SET && set_data[0] != 0) {
        // Find the highest offset used and set count accordingly
        // Profile is at 10 (1 byte), Calibration at 11 (2 bytes)
        // So we need to send at least 13 bytes to cover everything
        set_count = 14;
    }

    return send_cmd(device, cmd, axis, set_data, set_count);
}