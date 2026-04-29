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

#define SOCKET_PATH "/tmp/x56-daemon.sock"
#define MAX_DEVICES 8
#define CALIBRATE_DURATION_MS 5000
#define sizeof_array(array) sizeof(array)/sizeof(array[0])
#define sleepms(time) usleep(time*1000)
#define ENABLE_DEBUG 1
#if ENABLE_DEBUG == 1 
#define DEBUG_PRINT(...) do { fprintf(stderr, __VA_ARGS__); fflush(stderr); } while(0)
#define PRINT_PACKET(data, size) do {\
    DEBUG_PRINT("packet:\n");\
    for (size_t i =0; i <size; i++) {\
        DEBUG_PRINT("0x%02X ",((uint8_t*)(data))[i]);\
        if (i+1 > 1 && (i+1) % 8 == 0)\
            DEBUG_PRINT("\n");\
    }\
    DEBUG_PRINT("\n");\
} while(0)
#define PRINT_CURVE(curve, curve_size) do {\
    DEBUG_PRINT("curve_size: %5d bytes\n",curve_size);\
    if (curve_size > UINT8_MAX)\
        curve_size*=2;\
    for (size_t i =0; i < curve_size; i++) {\
        DEBUG_PRINT("0x%02X ",((uint8_t*)curve)[i]);\
        if (i+1 > 1 && (i+1) % 8 == 0)\
            DEBUG_PRINT("\n");\
    }\
    DEBUG_PRINT("\n");\
} while (0)
#else
#define DEBUG_PRINT(...) do {} while(0)
#define PRINT_PACKET(data, size) do {} while(0)
#define PRINT_CURVE(curve, curve_size) do {} while (0)
#endif

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
int cuve_size = 0;
int config_packet_size = 0;

//#if ENABLE_DEBUG
//void PRINT_PACKET(uint8_t *data,size_t size){
//    DEBUG_PRINT("packet:\n");
//    for (uint8_t i =0; i <size; i++) {
//        DEBUG_PRINT("0x%02X ",data[i]);
//        if (i+1 > 1 && (i+1) % 8 == 0)
//            DEBUG_PRINT("\n");
//    }
//    DEBUG_PRINT("\n");
//}
//#else
//void PRINT_PACKET(uint8_t *data, size_t size) {(void)data; (void)size;}
//#endif

static void select_axis(struct x56_dev *dev, uint8_t axis) {
    uint8_t pkt1[] = {0x0b, 0x03, 0x00, axis};
    uint8_t pkt2[] = {0x0b, 0x04, 0x00, axis};
    send_control(dev, 0x030b, pkt1, 4);
    send_control(dev, 0x030b, pkt2, 4);
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
    cfg->curve = (data[15] << 8) | data[16];
    cfg->profile = data[17];
    cfg->calibration = (data[18] << 8) | data[19];

    PRINT_PACKET(data, 64);
    DEBUG_PRINT("axis config\n");
    DEBUG_PRINT("xsat        = %4d\n",cfg->xsat);
    DEBUG_PRINT("ysat        = %4d\n",cfg->ysat);
    DEBUG_PRINT("deadband    = %4d\n",cfg->deadband);
    DEBUG_PRINT("curve       = %4d\n",cfg->curve);
    DEBUG_PRINT("profile     = %4d\n",cfg->profile);
    DEBUG_PRINT("calibration = %4d\n",cfg->calibration);
    return 0;
}

// Generate a curve lookup table based on axis configuration
// Uses power curve method: output = sign(input) * |input|^gamma
// gamma derived from curve param: gamma = curve/500 (500 = linear)
static void generate_curve_power(const configuration_t *config, uint16_t *curve, uint32_t size) {
    // Normalize config values to doubles
    double deadband = config->deadband / 1000.0;
    double curve_param = config->curve / 500.0;  // 1.0 = linear
    int profile = config->profile;  // 0 = S-curve (joystick), 1 = J-curve (dial)

    for (size_t i = 0; i < size; i++) {
        // Map index to normalized range [0, 1]
        double normalized = (double)i / (size - 1);

        // Handle direction inversion based on xsat
        // xsat < 500 means invert
        if (config->xsat < 500) {
            normalized = 1.0 - normalized;
        }

        // Handle profile (curve mirroring)
        // profile 0 = S-curve (mirror from center, for joysticks)
        // profile 1 = J-curve (one-sided, for dials)
        double signed_input;
        if (profile == 0) {
            // S-curve: map [0,1] to [-1,1] with center at 0.5
            signed_input = (normalized - 0.5) * 2.0;  // [-1, 1]
        } else {
            // J-curve: keep as [0,1]
            signed_input = normalized;
        }

        // Apply deadband to center zone
        double deadband_range = deadband;
        if (profile == 0) {
            // S-curve: symmetric deadband around center
            if (fabs(signed_input) < deadband_range) {
                signed_input = 0.0;
            } else {
                // Rescale to exclude deadband zone
                double sign = (signed_input > 0) ? 1.0 : -1.0;
                signed_input = sign * ((fabs(signed_input) - deadband_range) / (1.0 - deadband_range));
            }
        } else {
            // J-curve: deadband at start
            if (signed_input < deadband_range) {
                signed_input = 0.0;
            } else {
                signed_input = (signed_input - deadband_range) / (1.0 - deadband_range);
            }
        }

        // Apply power curve
        double sign = (signed_input >= 0) ? 1.0 : -1.0;
        double abs_val = fabs(signed_input);
        if (abs_val > 0.0) {
            abs_val = pow(abs_val, curve_param);
        }
        double curved = sign * abs_val;

        // Get maximum size of the curve based on the array and ysat
        double scale = config->ysat /1000.0f;        
        // Scale to output range based on the array size
        double max_output = (size - 1) * scale;

        // Map output range based on profile
        double output;
        if (profile == 0) {
            // S-curve: map [-1, 1] to [0, max_output]
            output = (curved + 1.0) * (max_output / 2.0);
        } else {
            // J-curve: map [0, 1] to [0, max_output]
            output = curved * max_output;
        }

        //Clamp and convert to full int
        uint16_t result = (uint16_t)(output + 0.5);
        if (result > max_output) result = (uint16_t)max_output;
        if (result < 0) result = 0;

        // Write as big-endian. this is what the X56 expects over bulk transfer port
        curve[i] = htobe16(result);
    }
}

// 8-bit version of power curve generator
static void generate_curve_power_8b(const configuration_t *config, uint8_t *curve, uint32_t size) {
    double deadband = config->deadband / 1000.0;
    double curve_param = config->curve / 500.0;
    int profile = config->profile;

    for (uint16_t i = 0; i < size; i++) {
        double normalized = (double)i / (size - 1);

        if (config->xsat < 500) {
            normalized = 1.0 - normalized;
        }

        double signed_input;
        if (profile == 0) {
            signed_input = (normalized - 0.5) * 2.0;
        } else {
            signed_input = normalized;
        }

        // Apply deadband
        double deadband_range = deadband;
        if (profile == 0) {
            if (fabs(signed_input) < deadband_range) {
                signed_input = 0.0;
            } else {
                double sign = (signed_input > 0) ? 1.0 : -1.0;
                signed_input = sign * ((fabs(signed_input) - deadband_range) / (1.0 - deadband_range));
            }
        } else {
            if (signed_input < deadband_range) {
                signed_input = 0.0;
            } else {
                signed_input = (signed_input - deadband_range) / (1.0 - deadband_range);
            }
        }

        // Apply power curve
        double sign = (signed_input >= 0) ? 1.0 : -1.0;
        double abs_val = fabs(signed_input);
        if (abs_val > 0.0) {
            abs_val = pow(abs_val, curve_param);
        }
        double curved = sign * abs_val;

        // Scale to 8-bit range [0, 255]
        double scale = config->ysat / 1000.0;
        uint8_t result;
        if (profile == 0) {
            result = (uint8_t)((curved + 1.0) * 127.5 * scale);
        } else {
            result = (uint8_t)(curved * 255.0 * scale);
        }

        curve[i] = (result > 255) ? 255 : result;
    }
}

// Main entry point - generates curve and stores it
// Returns 0 on success
// curve: pointer to curve array (uint8_t* or uint16_t* depending on curve type)
static int generate_axis_curve(const configuration_t *config, void *curve, uint32_t size) {
    if (!config || !curve || size == 0) {
        return -1;
    }
    switch (size)
    {
        case 1 ... 255:{      // 8bits or less
            generate_curve_power_8b(config,(uint8_t*)curve, size);
//            uint8_t* c = (uint8_t*)curve;
//            for (size_t i=0; i < size-1; i++) {
//                c[i] = i;
//            }
//            c[size-1] = (uint8_t)size|0xFF;
            break;
        }
        case 256 ... 65535: { //  larger than 8bits
            generate_curve_power(config,(uint16_t*)curve, size);
//            uint16_t* c = (uint16_t*)curve;
//            for (size_t i=0; i < size-1; i++) {
//                c[i] = htobe16((uint16_t)i);
//            }
//            c[size-1] = htobe16((uint16_t)size)|0xFF;
            break;
        }
        default:
            break;
    }
    return 0;
}

// Send curve via bulk transfer
// Returns bytes transferred on success, negative on error
static int send_curve_bulk(struct x56_dev *dev, uint8_t axis, void *curve, uint32_t size) {
    uint16_t transferred = 0;

    // Select axis first, then send raw curve data via bulk
    select_axis(dev, axis);

    uint8_t *bytes = (uint8_t*)curve;
    uint32_t remaining = size;

    while (remaining > 0) {
        uint16_t to_send = (remaining > 64) ? 64 : remaining;
        PRINT_PACKET(bytes,to_send);
        transferred = to_send;
        int ret = 0;
        // for while testing
        //int ret = send_bulk(dev, bytes, to_send, &transferred);
        if (ret < 0) {
            return ret;
        }
        bytes += transferred;
        remaining -= transferred;
        if (transferred == 0) {
            // No progress, avoid infinite loop
            return -1;
        }
    }

    return size;
}

static int set_axis_config(struct x56_dev *dev, uint8_t axis, configuration_t *cfg) {
    select_axis(dev, axis);

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
    pkt[14] = cfg->curve >> 8;
    pkt[15] = cfg->curve & 0xff;
    pkt[16] = cfg->profile;
    pkt[17] = cfg->calibration >> 8;
    pkt[18] = cfg->calibration & 0xff;

    return send_control(dev, 0x030b, pkt, 19);
}

static int set_axis_defaults(struct x56_dev *dev, uint8_t axis) {
    select_axis(dev, axis);

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
    select_axis(dev, axis);
    set_axis_defaults(dev, axis);

    uint32_t sum = 0;
    int count = 0;
    uint32_t start = time(NULL) * 1000;

    while ((time(NULL) * 1000 - start) < CALIBRATE_DURATION_MS) {
        uint8_t data[64];
        int ret = read_interrupt(dev, data, 64);
        if (ret > 0) {
            if (axis == 30 || axis == 31) {
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
        cfg.curve = 500;
        cfg.profile = 0;
        cfg.calibration = *cal_value;
        return set_axis_config(dev, axis, &cfg);
    }

    return -1;
}



static void *input_stream_thread(void *arg) {
    device_context_t *ctx = (device_context_t*)arg;
    struct x56_dev *dev = ctx->dev;

    while (ctx->streaming && running) {
        uint8_t data[64];
        int ret = read_interrupt(dev, data, 64);
        if (ret > 0 && ctx->client_fd > 0) {
            msg_t msg = {0};
            msg.device = dev->id;
            msg.msg_type = MSG_INPUT;
            memcpy(msg.data, data, 64);
            send(ctx->client_fd, &msg, sizeof(msg_t), 0);
        }
    }

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
            int ret = get_axis_config(dev, msg.axis, &cfg);
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
        // TODO: auto regenerate curve and store in curve array if the axis
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

            // Get current config
            configuration_t current_cfg = {0};
            int ret = get_axis_config(dev, msg.axis, &current_cfg);
            if (ret < 0) {
                error = -ret;
                break;
            }

            // Apply partial updates
            if (field_mask & CFG_XSAT) {
                current_cfg.xsat = *(uint16_t*)&msg.data[CFG_VALUE_XSAT];
                update_curve = 1;
            }
            if (field_mask & CFG_YSAT) {
                current_cfg.ysat = *(uint16_t*)&msg.data[CFG_VALUE_YSAT];
                update_curve = 1;
            }
            if (field_mask & CFG_DEADBAND) {
                current_cfg.deadband = *(uint16_t*)&msg.data[CFG_VALUE_DEADBAND];
                update_curve = 1;
            }
            if (field_mask & CFG_CURVE) {
                current_cfg.curve = *(uint16_t*)&msg.data[CFG_VALUE_CURVE];
                update_curve = 1;
            }
            if (field_mask & CFG_PROFILE) {
                current_cfg.profile = msg.data[CFG_VALUE_PROFILE];
                update_curve = 1;
            }
            if (field_mask & CFG_CALIBRATION) {
                current_cfg.calibration = msg.data[CFG_VALUE_CALIBRATION];
                update_curve = 1;
            }

            // Send updated config to device
            ret = set_axis_config(dev, msg.axis, &current_cfg);
            if (ret < 0) {
                error = -ret;
                break;
            }

            // Update stored config and optionally generate curve
            configuration_t *stored_cfg = NULL;
            if (dev->type == DEV_THROTTLE) {
                switch (msg.axis) {
                    case 0x30: stored_cfg = &configuration_throttle.Throttle1.config; break;
                    case 0x31: stored_cfg = &configuration_throttle.Throttle2.config; break;
                    case 0x32: stored_cfg = &configuration_throttle.Rotary1.config; break;
                    case 0x35: stored_cfg = &configuration_throttle.Rotary2.config; break;
                    case 0x36: stored_cfg = &configuration_throttle.Rotary4.config; break;
                    case 0x37: stored_cfg = &configuration_throttle.Rotary3.config; break;
                }
            } else if (dev->type == DEV_JOYSTICK) {
                switch (msg.axis) {
                    case 0x30: stored_cfg = &configuration_joystick.X.config; break;
                    case 0x31: stored_cfg = &configuration_joystick.Y.config; break;
                    case 0x35: stored_cfg = &configuration_joystick.Rz.config; break;
                }
            }

            if (stored_cfg) {
                *stored_cfg = current_cfg;

                // Generate and store curve if requested or config changed
//                if (flags & CFG_GENERATE_CURVE) {
                if (update_curve) {
                    if (dev->type == DEV_THROTTLE) {
                        switch (msg.axis) {
                            case 0x30: generate_axis_curve(stored_cfg, configuration_throttle.Throttle1.curve, sizeof_array(configuration_throttle.Throttle1.curve)); break;
                            case 0x31: generate_axis_curve(stored_cfg, configuration_throttle.Throttle2.curve, sizeof_array(configuration_throttle.Throttle2.curve)); break;
                            case 0x32: generate_axis_curve(stored_cfg, configuration_throttle.Rotary1.curve, sizeof_array(configuration_throttle.Rotary1.curve)); break;
                            case 0x35: generate_axis_curve(stored_cfg, configuration_throttle.Rotary2.curve, sizeof_array(configuration_throttle.Rotary2.curve)); break;
                            case 0x36: generate_axis_curve(stored_cfg, configuration_throttle.Rotary4.curve, sizeof_array(configuration_throttle.Rotary4.curve)); break;
                            case 0x37: generate_axis_curve(stored_cfg, configuration_throttle.Rotary3.curve, sizeof_array(configuration_throttle.Rotary3.curve)); break;
                        }
                    } else if (dev->type == DEV_JOYSTICK) {
                        switch (msg.axis) {
                            case 0x30: generate_axis_curve(stored_cfg, configuration_joystick.X.curve, sizeof_array(configuration_joystick.X.curve)); break;
                            case 0x31: generate_axis_curve(stored_cfg, configuration_joystick.Y.curve, sizeof_array(configuration_joystick.Y.curve)); break;
                            case 0x35: generate_axis_curve(stored_cfg, configuration_joystick.Rz.curve, sizeof_array(configuration_joystick.Rz.curve)); break;
                        }
                    }
                }
            }
            break;
        }
        case CMD_CURVE: {
            void *curve = NULL;
            uint32_t curve_size = 0;
            if (dev->type == DEV_THROTTLE) {
                switch (msg.axis) {
                    case 0x30: curve = &configuration_throttle.Throttle1.curve; curve_size = sizeof_array(configuration_throttle.Throttle1.curve); break;
                    case 0x31: curve = &configuration_throttle.Throttle2.curve; curve_size = sizeof_array(configuration_throttle.Throttle2.curve); break;
                    case 0x32: curve = &configuration_throttle.Rotary1.curve; curve_size = sizeof_array(configuration_throttle.Rotary1.curve); break;
                    case 0x35: curve = &configuration_throttle.Rotary2.curve; curve_size = sizeof_array(configuration_throttle.Rotary2.curve); break;
                    case 0x36: curve = &configuration_throttle.Rotary4.curve; curve_size = sizeof_array(configuration_throttle.Rotary4.curve); break;
                    case 0x37: curve = &configuration_throttle.Rotary3.curve; curve_size = sizeof_array(configuration_throttle.Rotary3.curve); break;
                    default: DEBUG_PRINT("unknown throttle axis: %u\n", msg.axis); break;
                }
            } else if (dev->type == DEV_JOYSTICK) {
                switch (msg.axis) {
                    case 0x30: curve = &configuration_joystick.X.curve; curve_size = sizeof_array(configuration_joystick.X.curve); break;
                    case 0x31: curve = &configuration_joystick.Y.curve; curve_size = sizeof_array(configuration_joystick.Y.curve); break;
                    case 0x35: curve = &configuration_joystick.Rz.curve; curve_size = sizeof_array(configuration_joystick.Rz.curve); break;
                    default: DEBUG_PRINT("unknown joystick axis: %u\n", msg.axis); break;
                }
            }
            if (curve && curve_size > 0) {
                int ret = 0;
                PRINT_CURVE(curve,curve_size);
                //int ret = send_curve_bulk(dev, msg.axis, curve, curve_size);
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
            // only will have to set calibration for hall axis
            configuration_t defaults = {
                .xsat = 1000,
                .ysat = 1000,
                .deadband = 0,
                .curve = 500,
                .profile = 0,
                .calibration = 0
            };
            if (dev->type == DEV_THROTTLE) {
                switch (msg.axis) {
                    case 0x30:{break;}
                    case 0x31:{break;}
                    case 0x32:{break;}
                    case 0x35:{break;}
                    case 0x37:{break;}
                    case 0x36:{break;}
                    default:break;
                }
            } else if (dev->type == DEV_JOYSTICK) {
                switch (msg.axis) {
                    case 0x30:{defaults.calibration = configuration_joystick.X.config.calibration; break;}
                    case 0x31:{defaults.calibration = configuration_joystick.Y.config.calibration; break;}
                    case 0x35:{ break;}
                    default:break;
                }
            }
            int ret = set_axis_config(dev, msg.axis, &defaults);
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
        cfg->curve = (data[15] << 8) | data[16];
        cfg->profile = data[17];
        cfg->calibration = (data[18] << 8) | data[19];
    } else {
        ret = get_control(dev, 0x0308, data, 64);
        DEBUG_PRINT("DEBUG: axis=%d ctrl ret=%d\n", axis, ret);
        if (ret > 37) {
            cfg->xsat = (data[27] << 8) | data[28];
            cfg->ysat = (data[29] << 8) | data[30];
            cfg->deadband = (data[31] << 8) | data[32];
            cfg->curve = (data[33] << 8) | data[34];
            cfg->profile = data[35];
            cfg->calibration = (data[36] << 8) | data[37];
        }
    }
    DEBUG_PRINT("DEBUG: axis=%d xsat=%d curve=%d\n", axis, cfg->xsat, cfg->curve);
}

static int device_callback(enum dev_type type, int connected, struct x56_dev *dev) {
    // early return when disconnect
    if (!connected || !dev) return 0;

    DEBUG_PRINT("DEBUG: Populating config for device %d\n", dev->id);
    switch (type) {
        case DEV_THROTTLE:
            get_axis_config(dev, 0x30, &configuration_throttle.Throttle1.config);
            get_axis_config(dev, 0x31, &configuration_throttle.Throttle2.config);
            get_axis_config(dev, 0x32, &configuration_throttle.Rotary1.config);
            get_axis_config(dev, 0x35, &configuration_throttle.Rotary2.config);
            get_axis_config(dev, 0x36, &configuration_throttle.Rotary4.config);
            get_axis_config(dev, 0x37, &configuration_throttle.Rotary3.config);
            generate_axis_curve(&configuration_throttle.Throttle1.config, configuration_throttle.Throttle1.curve, sizeof_array(configuration_throttle.Throttle1.curve));
            generate_axis_curve(&configuration_throttle.Throttle2.config, configuration_throttle.Throttle2.curve, sizeof_array(configuration_throttle.Throttle2.curve));
            generate_axis_curve(&configuration_throttle.Rotary1.config, configuration_throttle.Rotary1.curve, sizeof_array(configuration_throttle.Rotary1.curve));
            generate_axis_curve(&configuration_throttle.Rotary2.config, configuration_throttle.Rotary2.curve, sizeof_array(configuration_throttle.Rotary2.curve));           
            generate_axis_curve(&configuration_throttle.Rotary4.config, configuration_throttle.Rotary4.curve, sizeof_array(configuration_throttle.Rotary4.curve));           
            generate_axis_curve(&configuration_throttle.Rotary3.config, configuration_throttle.Rotary3.curve, sizeof_array(configuration_throttle.Rotary3.curve));           
            break;
        case DEV_JOYSTICK:
            get_axis_config(dev, 0x30, &configuration_joystick.X.config);
            get_axis_config(dev, 0x31, &configuration_joystick.Y.config);
            get_axis_config(dev, 0x35, &configuration_joystick.Rz.config);
            generate_axis_curve(&configuration_joystick.X.config, configuration_joystick.X.curve, sizeof_array(configuration_joystick.X.curve));
            generate_axis_curve(&configuration_joystick.Y.config, configuration_joystick.Y.curve, sizeof_array(configuration_joystick.Y.curve));
            generate_axis_curve(&configuration_joystick.Rz.config, configuration_joystick.Rz.curve, sizeof_array(configuration_joystick.Rz.curve));
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