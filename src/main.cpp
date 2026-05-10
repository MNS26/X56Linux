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
#include <pthread.h>
#include <time.h>
#include <math.h>

#include "usb.h"
#include "packet.h"
#include "common.h"
#include "configuration.h"
#include "curve.h"

#define ENABLE_DEBUG 0
#if ENABLE_DEBUG == 1 
#define DEBUG_PRINT(...) do {         \
        fprintf(stderr, __VA_ARGS__); \
        fflush(stderr);               \
} while (0)

#define PRINT_PACKET(data, size) do {                 \
    DEBUG_PRINT("packet:\n");                         \
    for (size_t i =0; i <size; i++) {                 \
        DEBUG_PRINT("0x%02X ",((uint8_t*)(data))[i]); \
        if (i+1 > 1 && (i+1) % 8 == 0)                \
            DEBUG_PRINT("\n");                        \
    }                                                 \
    DEBUG_PRINT("\n");                                \
} while (0)

#define PRINT_CURVE(curvature, curve_bytes) do {              \
    DEBUG_PRINT("curve_size: %5u bytes\n", (unsigned)curve_bytes); \
    for (size_t i = 0; i < curve_bytes; i++) {               \
        DEBUG_PRINT("0x%02X ", ((uint8_t*)curvature)[i]);    \
        if ((i + 1) % 8 == 0) DEBUG_PRINT("\n");              \
    }                                                           \
    DEBUG_PRINT("\n");                                          \
} while (0)

#else
#define DEBUG_PRINT(...) do {} while(0)
#define PRINT_PACKET(data, size) do {} while(0)
#define PRINT_CURVE(curvature, curve_size) do {} while(0)
#endif


#define SOCKET_PATH "/tmp/x56-daemon.sock"
#define MAX_DEVICES 8
#define CALIBRATE_DURATION_MS 5000
#define sizeof_array(array) sizeof(array)/sizeof(array[0])


// arrays for the different sizes
uint8_t curve_8b[1<<8];
uint16_t curve_10b[1<<10];
uint16_t curve_12b[1<<12];
uint16_t curve_16b[1<<16];

static volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}
typedef struct device_context {
    struct x56_dev *dev;
    pthread_t input_thread;
    int streaming;
    int client_fd;
    pthread_mutex_t lock;
} device_context_t;

static device_context_t device_contexts[MAX_DEVICES];
static configuration_joystick_t configuration_joystick;
static configuration_throttle_t configuration_throttle;

static int save_config (struct x56_dev *dev) {
    uint8_t packet[] = {0x01, 0x01};
    return send_control(dev, 0x0301,packet,2);
}

static void select_axis_curve(struct x56_dev *dev, uint8_t axis) {
    uint8_t select[] = {0x0b, 0x03, 0x00, axis};
    send_control(dev, 0x030b, select, 4);
}

static void select_axis_config(struct x56_dev *dev, uint8_t axis) {
    uint8_t select[] = {0x0b, 0x04, 0x00, axis};
    send_control(dev, 0x030b, select, 4);
}

static int get_axis_config(struct x56_dev *dev, uint8_t axis, configuration_t *cfg) {
    uint8_t select[] = {0x0b, 0x02, 0x00, axis};    
    send_control(dev, 0x030b, select, 4);

    uint8_t data[64] = {0};

    int ret = read_interrupt(dev, data, 64);

    if ((uint)ret != 0) return -1;
//    if (data[4]!=axis || data[8]!= axis) return -1;

    cfg->xsat = (data[9] << 8) | data[10];
    cfg->ysat = (data[11] << 8) | data[12];
    cfg->deadband = (data[13] << 8) | data[14];
    cfg->curvature = (data[15] << 8) | data[16];
    cfg->profile = data[17];
    cfg->calibration = (data[18] << 8) | data[19];

    PRINT_PACKET(data, 64);
    DEBUG_PRINT("axis config\n");
    DEBUG_PRINT("xsat        = %4d\n",cfg->xsat);
    DEBUG_PRINT("ysat        = %4d\n",cfg->ysat);
    DEBUG_PRINT("deadband    = %4d\n",cfg->deadband);
    DEBUG_PRINT("curve       = %4d\n",cfg->curvature);
    DEBUG_PRINT("profile     = %4d\n",cfg->profile);
    DEBUG_PRINT("calibration = %4d\n",cfg->calibration);
    return 0;
}

// Send curve via bulk transfer
// Returns bytes transferred on success, negative on error
static int send_curve_bulk(struct x56_dev *dev, uint8_t axis, void *curve, uint32_t size) {
    select_axis_curve(dev, axis);
    DEBUG_PRINT("DEBUG: Sending %u bytes of curve data\n", size);
    int ret = send_bulk_curve(dev, (uint8_t*)curve, size);
    if (ret < 0) {
        DEBUG_PRINT("ERROR: Failed to send curve data: %d (%s)\n", ret, libusb_error_name(ret));
        return ret;
    }
    DEBUG_PRINT("DEBUG: Successfully sent %d bytes of curve data\n", ret);
    return ret;
}

static int set_axis_config(struct x56_dev *dev, uint8_t axis, configuration_t *cfg) {
    select_axis_config(dev, axis);

    uint8_t pkt[19] = {0};
    pkt[0] = 0x0b;
    pkt[1] = 0x01;
    pkt[2] = 0x00;
    pkt[3] = axis;
    pkt[4] = 0x00;
    pkt[5] = 0x01;
    pkt[6] = 0x00;
    pkt[7] = axis;
    pkt[8] = cfg->xsat >> 8;
    pkt[9] = cfg->xsat & 0xff;
    pkt[10] = cfg->ysat >> 8;
    pkt[11] = cfg->ysat & 0xff;
    pkt[12] = cfg->deadband >> 8;
    pkt[13] = cfg->deadband & 0xff;
    pkt[14] = cfg->curvature >> 8;
    pkt[15] = cfg->curvature & 0xff;
    pkt[16] = cfg->profile;
    pkt[17] = cfg->calibration >> 8;
    pkt[18] = cfg->calibration & 0xff;

    return send_control(dev, 0x030b, pkt, 19);
}

static int set_axis_defaults(struct x56_dev *dev, uint8_t axis) {
    select_axis_config(dev, axis);

    uint8_t pkt[19] = {0};
    pkt[0] = 0x0b;
    pkt[1] = 0x01;
    pkt[2] = 0x00;
    pkt[3] = axis;
    pkt[4] = 0x00;
    pkt[5] = 0x00;
    pkt[6] = 0x00;
    pkt[7] = 0x00;
    pkt[8] = 0x03;
    pkt[9] = 0xe8;
    pkt[10] = 0x03;
    pkt[11] = 0xe8;
    pkt[12] = 0x00;
    pkt[13] = 0x00;
    pkt[14] = 0x01;
    pkt[15] = 0xf4;
    pkt[16] = 0x00;

    return send_control(dev, 0x030b, pkt, 19);
}

static int calibrate_axis(struct x56_dev *dev, uint8_t axis, uint16_t *cal_value) {
    select_axis_config(dev, axis);
    set_axis_defaults(dev, axis);

    uint32_t sum = 0;
    int count = 0;
    uint32_t start = time(NULL) * 1000;

    while ((time(NULL) * 1000 - start) < CALIBRATE_DURATION_MS) {
        uint8_t data[64];
        int ret = read_interrupt(dev, data, 64);
        if (ret > 0) {
            if (axis == 0x30 || axis == 0x31) {
                uint16_t val = (data[1] << 8) | data[0];
                sum += val;
                count++;
            }
        }
    }

    if (count > 0) {
        *cal_value = sum / count;
        configuration_t cfg = {0};
        cfg.xsat = 1000;
        cfg.ysat = 1000;
        cfg.deadband = 0;
        cfg.curvature = 500;
        cfg.profile = 0;
        cfg.calibration = *cal_value;
        return set_axis_config(dev, axis, &cfg);
    }

    return -1;
}



static void *input_stream_thread(void *arg) {
    device_context_t *ctx = (device_context_t*)arg;
    
    while (ctx->streaming && running) {
        // Check if device is still valid
        if (!ctx->dev || !ctx->dev->active || !ctx->dev->handle) {
            break;
        }
        
        uint8_t data[64];
        int ret = read_interrupt(ctx->dev, data, 64);
        if (ret >= 0 && ctx->client_fd > 0) {
            msg_t msg = {0};
            msg.device = ctx->dev->id;
            msg.msg_type = MSG_INPUT;
            memcpy(msg.data, data, 64);
            send(ctx->client_fd, &msg, sizeof(msg_t), 0);
        } else if (ret < 0) {
            // Device error, stop streaming
            break;
        }
    }
    
    ctx->streaming = 0;
    return NULL;
}

static void handle_client(int client_fd, struct usb_ctx *ctx) {
    msg_t msg;
    memset(&msg, 0, sizeof(msg_t));

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ssize_t p_len = recv(client_fd, &msg, sizeof(msg_t), 0);
    if (p_len != sizeof(msg_t)) {
        if (p_len < 0) perror("recv");
        return;
    }

    if (msg.device > 2) {
        return;
    }

    msg_t response = {0};
    response.device = msg.device;
    response.msg_type = MSG_CMD;

    // For device 0, apply to all active devices
    uint8_t start_dev = (msg.device == 0) ? 1 : msg.device;
    uint8_t end_dev = (msg.device == 0) ? 2 : msg.device;
    int error = 0;

    for (uint8_t d = start_dev; d <= end_dev; d++) {
        struct x56_dev *dev = ctx->devices[d - 1];
        if (!dev || !dev->active) {
            continue;
        }

        // Let libusb process any pending events
        struct timeval tv = {0, 1000};
        libusb_handle_events_timeout(ctx->ctx, &tv);

        response.device = d;

        switch (msg.cmd) {
        case CMD_GET: {
            configuration_t cfg = {0};
            int ret = 0;
//            ret = get_axis_config(dev, msg.axis, &cfg);
            if (dev->type == DEV_THROTTLE) {
                switch (msg.axis) {
                    case 0x30: cfg = configuration_throttle.Throttle1.config; break;
                    case 0x31: cfg = configuration_throttle.Throttle2.config; break;
                    case 0x32: cfg = configuration_throttle.Rotary1.config; break;
                    case 0x35: cfg = configuration_throttle.Rotary2.config; break;
                    case 0x36: cfg = configuration_throttle.Rotary4.config; break;
                    case 0x37: cfg = configuration_throttle.Rotary3.config; break;
                }
            } else if (dev->type == DEV_JOYSTICK) {
                switch (msg.axis) {
                    case 0x30: cfg = configuration_joystick.X.config; break;
                    case 0x31: cfg = configuration_joystick.Y.config; break;
                    case 0x35: cfg = configuration_joystick.Rz.config; break;
                }
            }
            if (ret == 0) {
                response.cmd = CMD_GET;
                response.axis = msg.axis;
                memcpy(response.data, &cfg, sizeof(configuration_t));
            } else {
                response.data[0] = -ret;
            }
            send(client_fd, &response, sizeof(msg_t), 0);
            break;  // Allow iterating to next device for device=0
        }
        case CMD_SET: {
            // data[0] = bitmask of fields to update
            // data[1] = flags (CFG_*)
            // data[2+] = values (16-bit aligned)
            uint8_t field_mask = msg.data[0];
            uint8_t flags = msg.data[1];
            uint8_t update_curve = 0;

            if (field_mask == 0) {
                // No fields to update
                break;
            }


            // Update stored config and update generate curve
            configuration_t *current_cfg = NULL;
            if (dev->type == DEV_THROTTLE) {
                switch (msg.axis) {
                    case 0x30: current_cfg = &configuration_throttle.Throttle1.config; break;
                    case 0x31: current_cfg = &configuration_throttle.Throttle2.config; break;
                    case 0x32: current_cfg = &configuration_throttle.Rotary1.config; break;
                    case 0x35: current_cfg = &configuration_throttle.Rotary2.config; break;
                    case 0x36: current_cfg = &configuration_throttle.Rotary4.config; break;
                    case 0x37: current_cfg = &configuration_throttle.Rotary3.config; break;
                }
            } else if (dev->type == DEV_JOYSTICK) {
                switch (msg.axis) {
                    case 0x30: current_cfg = &configuration_joystick.X.config; break;
                    case 0x31: current_cfg = &configuration_joystick.Y.config; break;
                    case 0x35: current_cfg = &configuration_joystick.Rz.config; break;
                }
            }
            // Get current config
//            configuration_t current_cfg = {0};
//            int ret = get_axis_config(dev, msg.axis, &current_cfg);
//            if (ret < 0) {
//                error = -ret;
//                break;
//            }

            // Apply partial updates
            if (field_mask & CFG_XSAT) {
                current_cfg->xsat = *(uint16_t*)&msg.data[CFG_VALUE_XSAT];
                update_curve = 1;
            }
            if (field_mask & CFG_YSAT) {
                current_cfg->ysat = *(uint16_t*)&msg.data[CFG_VALUE_YSAT];
                update_curve = 1;
            }
            if (field_mask & CFG_DEADBAND) {
                current_cfg->deadband = *(uint16_t*)&msg.data[CFG_VALUE_DEADBAND];
                update_curve = 1;
            }
            if (field_mask & CFG_CURVE) {
                current_cfg->curvature = *(uint16_t*)&msg.data[CFG_VALUE_CURVE];
                update_curve = 1;
            }
            if (field_mask & CFG_PROFILE) {
                current_cfg->profile = msg.data[CFG_VALUE_PROFILE];
                update_curve = 1;
            }
            if (field_mask & CFG_CALIBRATION) {
                current_cfg->calibration = msg.data[CFG_VALUE_CALIBRATION];
//                update_curve = 1;
            }

            // Send updated config to device
            int ret = set_axis_config(dev, msg.axis, current_cfg);
            if (ret < 0) {
                error = -ret;
                break;
            }


            // Update config 
//            if (stored_cfg) {
//                *stored_cfg = current_cfg;

                // Generate and store curve if requested or config changed
//                if (flags & CFG_GENERATE_CURVE) {
                if (update_curve) {
                    if (dev->type == DEV_THROTTLE) {
                        switch (msg.axis) {
                            //case 0x30: generate_axis_curve(current_cfg, configuration_throttle.Throttle1.curve, configuration_throttle.Throttle1.bits, 0); break;
                            //case 0x31: generate_axis_curve(current_cfg, configuration_throttle.Throttle2.curve, configuration_throttle.Throttle2.bits, 0); break;
                            //case 0x32: generate_axis_curve(current_cfg, configuration_throttle.Rotary1.curve, configuration_throttle.Rotary1.bits, 0); break;
                            //case 0x35: generate_axis_curve(current_cfg, configuration_throttle.Rotary2.curve, configuration_throttle.Rotary2.bits, 0); break;
                            //case 0x36: generate_axis_curve(current_cfg, configuration_throttle.Rotary4.curve, configuration_throttle.Rotary4.bits, 0); break;
                            //case 0x37: generate_axis_curve(current_cfg, configuration_throttle.Rotary3.curve, configuration_throttle.Rotary3.bits, 0); break;
                            
                            //case 0x30: generate_axis_curve(current_cfg, configuration_throttle.Throttle1.curve, sizeof_array(configuration_throttle.Throttle1.curve)); break;
                            //case 0x31: generate_axis_curve(current_cfg, configuration_throttle.Throttle2.curve, sizeof_array(configuration_throttle.Throttle2.curve)); break;
                            //case 0x32: generate_axis_curve(current_cfg, configuration_throttle.Rotary1.curve, sizeof_array(configuration_throttle.Rotary1.curve)); break;
                            //case 0x35: generate_axis_curve(current_cfg, configuration_throttle.Rotary2.curve, sizeof_array(configuration_throttle.Rotary2.curve)); break;
                            //case 0x36: generate_axis_curve(current_cfg, configuration_throttle.Rotary4.curve, sizeof_array(configuration_throttle.Rotary4.curve)); break;
                            //case 0x37: generate_axis_curve(current_cfg, configuration_throttle.Rotary3.curve, sizeof_array(configuration_throttle.Rotary3.curve)); break;
                            default: break;
                        }
                    } else if (dev->type == DEV_JOYSTICK) {
                        switch (msg.axis) {
                            //case 0x30: generate_axis_curve(current_cfg, configuration_joystick.X.curve, configuration_joystick.X.bits, 0); break;
                            //case 0x31: generate_axis_curve(current_cfg, configuration_joystick.Y.curve, configuration_joystick.Y.bits, 0); break;
                            //case 0x35: generate_axis_curve(current_cfg, configuration_joystick.Rz.curve, configuration_joystick.Rz.bits, 0); break;
                            
                            //case 0x30: generate_axis_curve(current_cfg, configuration_joystick.X.curve, sizeof_array(configuration_joystick.X.curve)); break;
                            //case 0x31: generate_axis_curve(current_cfg, configuration_joystick.Y.curve, sizeof_array(configuration_joystick.Y.curve)); break;
                            //case 0x35: generate_axis_curve(current_cfg, configuration_joystick.Rz.curve, sizeof_array(configuration_joystick.Rz.curve)); break;
                            default: break;
                        }
                    }
                }
//            }
            break;

        }
        case CMD_CURVE: {
            void *curve = NULL;
            configuration_t *config = NULL;
            uint8_t* curve_bits = NULL;
            uint32_t curve_bytes = 0;
            if (dev->type == DEV_THROTTLE) {
                switch (msg.axis) {
                    case 0x30: config = &configuration_throttle.Throttle1.config; curve = &curve_10b; curve_bits = &configuration_throttle.Throttle1.bits; break;
                    case 0x31: config = &configuration_throttle.Throttle2.config; curve = &curve_10b; curve_bits = &configuration_throttle.Throttle2.bits; break;
                    case 0x32: config = &configuration_throttle.Rotary1.config; curve = &curve_8b; curve_bits = &configuration_throttle.Rotary1.bits; break;
                    case 0x35: config = &configuration_throttle.Rotary2.config; curve = &curve_8b; curve_bits = &configuration_throttle.Rotary2.bits; break;
                    case 0x36: config = &configuration_throttle.Rotary4.config; curve = &curve_8b; curve_bits = &configuration_throttle.Rotary4.bits; break;
                    case 0x37: config = &configuration_throttle.Rotary3.config; curve = &curve_8b; curve_bits = &configuration_throttle.Rotary3.bits; break;
                    default: DEBUG_PRINT("unknown throttle axis: %u\n", msg.axis); break;
                }
            } else if (dev->type == DEV_JOYSTICK) {
                switch (msg.axis) {
                    case 0x30: config = &configuration_joystick.X.config; curve = &curve_16b; curve_bits = &configuration_joystick.X.bits; break;
                    case 0x31: config = &configuration_joystick.Y.config; curve = &curve_16b; curve_bits = &configuration_joystick.Y.bits; break;
                    case 0x35: config = &configuration_joystick.Rz.config; curve = &curve_12b; curve_bits = &configuration_joystick.Rz.bits; break;
                    default: DEBUG_PRINT("unknown joystick axis: %u\n", msg.axis); break;
                }
            }

            generate_axis_curve(config,curve,*curve_bits);
            uint8_t typesize = (*curve_bits == 8) ? 1 : 2;
            curve_bytes = (1 << *curve_bits) * typesize;
            if (curve && curve_bytes > 0) {
                int ret = 0;
//#if ENABLE_DEBUG == 1
                PRINT_CURVE(curve,curve_bytes);
//#else
                ret = send_curve_bulk(dev, msg.axis, curve, curve_bytes);
                set_axis_config(dev,msg.axis,config);
                save_config(dev);
//#endif
                if (ret < 0) error = -ret;
            } else {
                DEBUG_PRINT("curve not found for axis %u\n", msg.axis);
                error = EINVAL;
            }
            break;
        }
        case CMD_CALIBRATE: {
            uint16_t cal_value = 0;
            int ret = calibrate_axis(dev, msg.axis, &cal_value);
            if (ret == 0) {
                response.cmd = CMD_CALIBRATE;
                response.axis = msg.axis;
                configuration_t cfg = {0};
                cfg.calibration = cal_value;
                memcpy(response.data, &cfg, sizeof(configuration_t));
            } else {
                response.data[0] = -ret;
            }
            send(client_fd, &response, sizeof(msg_t), 0);
            return;
        }
        case CMD_RESET: {
                void *curve = NULL;
                size_t curve_bytes = 0;

            // only will have to set calibration for hall axis
            configuration_t defaults = {
                .xsat = 1000,
                .ysat = 1000,
                .deadband = 0,
                .curvature = 500,
                .profile = 0,
                .calibration = 0
            };
//            if (dev->type == DEV_THROTTLE) {
//                switch (msg.axis) {
//                    case 0x30: curve = &configuration_throttle.Throttle1.curve; break;
//                    case 0x31: curve = &configuration_throttle.Throttle2.curve; break;
//                    case 0x32: curve = &configuration_throttle.Rotary1.curve; break;
//                    case 0x35: curve = &configuration_throttle.Rotary2.curve; break;
//                    case 0x36: curve = &configuration_throttle.Rotary4.curve; break;
//                    case 0x37: curve = &configuration_throttle.Rotary3.curve; break;
//                    default:break;
//                }
//            } else if (dev->type == DEV_JOYSTICK) {
//                switch (msg.axis) {
//                    case 0x30:{curve = &configuration_joystick.X.curve; break;}
//                    case 0x31:{curve = &configuration_joystick.Y.curve; break;}
//                    case 0x35:{curve = &configuration_joystick.Rz.curve; break;}
//                    default:break;
//                }
//            }
            int ret = 0;
//            ret |= generate_axis_curve(&defaults, curve, curve_bytes);
//            send_curve_bulk(dev, msg.axis, curve, curve_bytes);
//            ret |= set_axis_config(dev, msg.axis, &defaults);
            if (ret < 0) error = -ret;
            break;
        }
        case CMD_RGB: {
            DEBUG_PRINT("DEBUG: setting RGB %d,%d,%d on device %d\n", msg.data[0], msg.data[1], msg.data[2], d);
            // First verify device is still valid by checking handle
            DEBUG_PRINT("DEBUG: device id=%d handle=%p type=%d active=%d\n", dev->id, (void*)dev->handle, dev->type, dev->active);
            int ret = usb_set_rgb(dev, msg.data[0], msg.data[1], msg.data[2]);
            DEBUG_PRINT("DEBUG: RGB returned %d\n", ret);
            if (ret < 0) error = -ret;
            break;
        }
        case CMD_RGB_SAVE: {
            DEBUG_PRINT("DEBUG: saving RGB colors on device %d\n", d);
            // First verify device is still valid by checking handle
            DEBUG_PRINT("DEBUG: device id=%d handle=%p type=%d active=%d\n", dev->id, (void*)dev->handle, dev->type, dev->active);
            int ret = usb_save_rgb(dev);
            DEBUG_PRINT("DEBUG: SAVE_RGB returned %d\n", ret);
            if (ret < 0) error = -ret;
            break;
        }
        case CMD_START_INPUT: {
            device_contexts[d-1].streaming = 1;
            device_contexts[d-1].client_fd = client_fd;
            pthread_create(&device_contexts[d-1].input_thread, NULL, input_stream_thread, &device_contexts[d-1]);
            break;
        }
        case CMD_STOP_INPUT: {
            device_contexts[d-1].streaming = 0;
            pthread_join(device_contexts[d-1].input_thread, NULL);
            break;
        }
        case CMD_LIST: {
            DEBUG_PRINT("DEBUG: CMD_LIST for device %d\n", d);
            response.cmd = CMD_LIST;
            response.device = d;
            response.data[0] = dev->type;
            response.data[1] = dev->active ? 1 : 0;
            send(client_fd, &response, sizeof(msg_t), 0);
            break;
        }
        default:
            error = EINVAL;
            break;
        }
    }

    // For SET commands and single-response commands, send response
    if (msg.cmd == CMD_SET || (msg.cmd != CMD_GET && msg.cmd != CMD_START_INPUT && msg.cmd != CMD_CALIBRATE && msg.cmd != CMD_LIST)) {
        response.data[0] = error;
        send(client_fd, &response, sizeof(msg_t), 0);
    }
}

static int setup_socket(void) {
    unlink(SOCKET_PATH);

    DEBUG_PRINT("DEBUG: creating socket...\n");
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    DEBUG_PRINT("DEBUG: socket created, fd=%d\n", fd);

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    DEBUG_PRINT("DEBUG: binding to %s...\n", SOCKET_PATH);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    DEBUG_PRINT("DEBUG: bind done\n");

    chmod(SOCKET_PATH, 0666);

    if (listen(fd, 5) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

// is not usefull in practice. get_axis_config does the same and is simpler.
static void populate_axis_config(struct x56_dev *dev, uint8_t axis, configuration_t *cfg) {
    uint8_t pkt1[] = {0x0b, 0x03, 0x00, axis};
    uint8_t pkt2[] = {0x0b, 0x04, 0x00, axis};
    uint8_t select[] = {0x0b, 0x02, 0x00, axis};

    send_control(dev, 0x030b, pkt1, 4);
    send_control(dev, 0x030b, pkt2, 4);
    send_control(dev, 0x030b, select, 4);

    usleep(20000);

    uint8_t data[64] = {0};
    int ret = read_interrupt(dev, data, 64);
    DEBUG_PRINT("DEBUG: axis=%d intr ret=%d\n", axis, ret);

    if (ret > 21 && data[0] == 0xff && data[1] == 0x0b) {
        cfg->xsat = (data[9] << 8) | data[10];
        cfg->ysat = (data[11] << 8) | data[12];
        cfg->deadband = (data[13] << 8) | data[14];
        cfg->curvature = (data[15] << 8) | data[16];
        cfg->profile = data[17];
        cfg->calibration = (data[18] << 8) | data[19];
    } else {
        ret = get_control(dev, 0x0308, data, 64);
        DEBUG_PRINT("DEBUG: axis=%d ctrl ret=%d\n", axis, ret);
        if (ret > 37) {
            cfg->xsat = (data[27] << 8) | data[28];
            cfg->ysat = (data[29] << 8) | data[30];
            cfg->deadband = (data[31] << 8) | data[32];
            cfg->curvature = (data[33] << 8) | data[34];
            cfg->profile = data[35];
            cfg->calibration = (data[36] << 8) | data[37];
        }
    }
    DEBUG_PRINT("DEBUG: axis=%d xsat=%d curve=%d\n", axis, cfg->xsat, cfg->curvature);
}

static int device_callback(enum dev_type type, int connected, struct x56_dev *dev) {
    // early return when disconnect
    if (!connected || !dev) {
        fprintf(stdout, "Device removed (type=%d)\n", type);
        // Stop any active input streams for this device
        for (int i = 0; i < MAX_DEVICES; i++) {
            if (device_contexts[i].dev && device_contexts[i].dev->type == type) {
                device_contexts[i].streaming = 0;
                if (device_contexts[i].input_thread) {
                    pthread_join(device_contexts[i].input_thread, NULL);
                    device_contexts[i].input_thread = 0;
                }
                device_contexts[i].dev = NULL;
                device_contexts[i].client_fd = -1;
                break;
            }
        }
        return 0;
    }
    fprintf(stdout, "Device with id: %d connected\n", dev->id);

    // Store device in device_contexts
    if (dev->id >= 1 && dev->id <= MAX_DEVICES) {
        device_contexts[dev->id - 1].dev = dev;
        device_contexts[dev->id - 1].streaming = 0;
        device_contexts[dev->id - 1].client_fd = -1;
    }

    // on 1st interupt read it doesnt return axis data
    uint8_t dummy[64] = {0};
    read_interrupt(dev, dummy, 64);
    DEBUG_PRINT("DEBUG: Populating config for device %d\n", dev->id);
    switch (type) {
        case DEV_THROTTLE:
            get_axis_config(dev, 0x30, &configuration_throttle.Throttle1.config);
            get_axis_config(dev, 0x31, &configuration_throttle.Throttle2.config);
            get_axis_config(dev, 0x32, &configuration_throttle.Rotary1.config);
            get_axis_config(dev, 0x35, &configuration_throttle.Rotary2.config);
            get_axis_config(dev, 0x36, &configuration_throttle.Rotary4.config);
            get_axis_config(dev, 0x37, &configuration_throttle.Rotary3.config);

            //generate_axis_curve(&configuration_throttle.Throttle1.config, configuration_throttle.Throttle1.curve, configuration_throttle.Throttle1.bits, 0);
            //generate_axis_curve(&configuration_throttle.Throttle2.config, configuration_throttle.Throttle2.curve, configuration_throttle.Throttle2.bits, 0);
            //generate_axis_curve(&configuration_throttle.Rotary1.config, configuration_throttle.Rotary1.curve, configuration_throttle.Rotary1.bits, 0);
            //generate_axis_curve(&configuration_throttle.Rotary2.config, configuration_throttle.Rotary2.curve, configuration_throttle.Rotary2.bits, 0);
            //generate_axis_curve(&configuration_throttle.Rotary4.config, configuration_throttle.Rotary4.curve, configuration_throttle.Rotary4.bits, 0);
            //generate_axis_curve(&configuration_throttle.Rotary3.config, configuration_throttle.Rotary3.curve, configuration_throttle.Rotary3.bits, 0);
            
            //generate_axis_curve(&configuration_throttle.Throttle1.config, configuration_throttle.Throttle1.curve, sizeof_array(configuration_throttle.Throttle1.curve));
            //generate_axis_curve(&configuration_throttle.Throttle2.config, configuration_throttle.Throttle2.curve, sizeof_array(configuration_throttle.Throttle2.curve));
            //generate_axis_curve(&configuration_throttle.Rotary1.config, configuration_throttle.Rotary1.curve, sizeof_array(configuration_throttle.Rotary1.curve));
            //generate_axis_curve(&configuration_throttle.Rotary2.config, configuration_throttle.Rotary2.curve, sizeof_array(configuration_throttle.Rotary2.curve));           
            //generate_axis_curve(&configuration_throttle.Rotary4.config, configuration_throttle.Rotary4.curve, sizeof_array(configuration_throttle.Rotary4.curve));           
            //generate_axis_curve(&configuration_throttle.Rotary3.config, configuration_throttle.Rotary3.curve, sizeof_array(configuration_throttle.Rotary3.curve));           
            break;
        case DEV_JOYSTICK:
            get_axis_config(dev, 0x30, &configuration_joystick.X.config);
            get_axis_config(dev, 0x31, &configuration_joystick.Y.config);
            get_axis_config(dev, 0x35, &configuration_joystick.Rz.config);
            
            //generate_axis_curve(&configuration_joystick.X.config, configuration_joystick.X.curve, configuration_joystick.X.bits, 0);
            //generate_axis_curve(&configuration_joystick.Y.config, configuration_joystick.Y.curve, configuration_joystick.Y.bits, 0);
            //generate_axis_curve(&configuration_joystick.Rz.config, configuration_joystick.Rz.curve, configuration_joystick.Rz.bits, 0);
            
            //generate_axis_curve(&configuration_joystick.X.config, configuration_joystick.X.curve, sizeof_array(configuration_joystick.X.curve));
            //generate_axis_curve(&configuration_joystick.Y.config, configuration_joystick.Y.curve, sizeof_array(configuration_joystick.Y.curve));
            //generate_axis_curve(&configuration_joystick.Rz.config, configuration_joystick.Rz.curve, sizeof_array(configuration_joystick.Rz.curve));
            break;
        case DEV_NONE:
        default:
            break;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    openlog("x56d", LOG_PID, LOG_DAEMON);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    memset(device_contexts, 0, sizeof(device_contexts));

    struct usb_ctx *ctx = usb_init_ctx();
    if (!ctx) {
        syslog(LOG_ERR, "Failed to init USB");
        return 1;
    }

    usb_scan_devices(ctx);

    for (int i = 0; i < ctx->device_count; i++) {
        struct x56_dev *dev = ctx->devices[i];
        if (dev && dev->active) {
            device_callback(dev->type, 1, dev);
        }
    }

    if (usb_hotplug_init(ctx, device_callback) < 0) {
        syslog(LOG_WARNING, "Hotplug not available");
    }

    DEBUG_PRINT("DEBUG: about to setup socket\n");
    fflush(stderr);

    int sock_fd = setup_socket();
    DEBUG_PRINT("DEBUG: socket setup done, sock_fd=%d\n", sock_fd);
    fflush(stderr);
    if (sock_fd < 0) {
        syslog(LOG_ERR, "Failed to setup socket");
        usb_free_ctx(ctx);
        return 1;
    }

    //syslog(LOG_INFO, "X-56 daemon running on %s", SOCKET_PATH);
    DEBUG_PRINT("DEBUG: entering main loop, sock_fd=%d\n", sock_fd);
    fflush(stderr);

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

    for (int i = 0; i < MAX_DEVICES; i++) {
        if (device_contexts[i].streaming) {
            device_contexts[i].streaming = 0;
            pthread_join(device_contexts[i].input_thread, NULL);
        }
    }

    close(sock_fd);
    unlink(SOCKET_PATH);
    usb_free_ctx(ctx);
    syslog(LOG_INFO, "X-56 daemon stopped");
    closelog();

    return 0;
}