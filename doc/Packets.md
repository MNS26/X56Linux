# X-56 Daemon Protocol Documentation

## Socket Communication

The daemon listens on Unix socket: `/run/x56-daemon.sock`

### Connection (C)

```c
int fd = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr = { .sun_family = AF_UNIX };
strncpy(addr.sun_path, "/run/x56-daemon.sock", sizeof(addr.sun_path) - 1);
connect(fd, (struct sockaddr*)&addr, sizeof(addr));
```

### Send/Receive

- **Send:** Write exactly `sizeof(packet_t)` bytes (72 bytes) to socket
- **Receive:** Read exactly `sizeof(packet_t)` bytes from socket

Set a receive timeout to avoid blocking:

```c
struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
```

---

## Packet Structure

**Total size: 72 bytes** (includes 2 bytes struct padding)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | `devices` | Bitmask: bit 0 = device 1, bit 1 = device 2, etc. |
| 1 | 1 | `expecting_data` | 1 = client expects response data back |
| 2 | 1 | `last_packet` | 1 = final packet in a sequence |
| 3 | 2 | `w_value` | USB control transfer wValue (little-endian) |
| 5 | 64 | `data` | Payload data |
| 69 | 1 | `crc` | CRC-8 checksum (calculated over bytes 0-68) |

---

## Device IDs

- **Device 1**: Throttle (idProduct: 0xa221)
- **Device 2**: Joystick (idProduct: 0x2221)

Device IDs are fixed by device type, not configurable.

---

## USB Commands

### 1. Set RGB Color

Used to change the RGB lighting color on the device.

| Field | Value |
|-------|-------|
| `data[0]` | 0x09 |
| `data[1]` | 0x00 |
| `data[2]` | 0x03 |
| `data[3]` | R (0-255) |
| `data[4]` | G (0-255) |
| `data[5]` | B (0-255) |

**Should be followed by End Packet. (in reality it works without too)**

---

### 2. End Packet (required after RGB)

Signals the end of a command sequence.

| Field | Value |
|-------|-------|
| `w_value` | 0x0300 |
| `last_packet` | 1 |
| `data[0]` | 0x01 |
| `data[1]` | 0x01 |

---

### 3. Get Axis Configuration

Retrieves the current configuration for a specific axis.

| Field | Value |
|-------|-------|
| `w_value` | 0x0202 |
| `expecting_data` | 1 |
| `data[0]` | 0x02 |
| `data[1]` | 0x00 |
| `data[2]` | 0x0b |
| `data[3]` | axis_id |

**Axis IDs:**
- Joystick: 30 = X, 31 = Y, 35 = Z
- Throttle: 30 = throttle 1, 31 = throttle 2, 32 = rotary 1, 35 = rotary 2, 36 = rotary 3, 37 = rotary 4

**Response Data Layout:**
| Offset | Field | Description |
|--------|-------|-------------|
| 4 | axis | The axis ID |
| 9-10 | xsat | X saturation (0-1000) |
| 11-12 | ysat | Y saturation (0-1000) |
| 13-14 | deadband | Deadband (0-1000) |
| 15-16 | curve | Curvature (0-1000) |
| 17 | profile | 0x01 = J, 0x00 = S |
| 18-19 | hall_call | Hall effect zero calibration |

---

### 4. Set Axis Configuration

Configures an axis with saturation, deadband, curve, and profile settings.

| Field | Value |
|-------|-------|
| `w_value` | 0x0100 |
| `data[0]` | 0x01 |
| `data[1]` | 0x00 |
| `data[3]` | axis_id |
| `data[4-7]` | axis_id repeated |
| `data[8]` | 0x01 |
| `data[9-10]` | xsat (uint16, little-endian) |
| `data[11-12]` | ysat (uint16, little-endian) |
| `data[13-14]` | deadband (uint16, little-endian) |
| `data[15-16]` | curve (uint16, little-endian) |
| `data[17]` | profile (0x01 = J, 0x00 = S) |
| `data[18-19]` | hall_call (uint16, usually 0) |

**Default values:** xsat=1000, ysat=1000, curve=500, deadband=0, profile=S

---

## CRC-8 Algorithm

**Polynomial: 0x07**  
**Initial value: 0x00**

```c
uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

### Calculating Packet CRC

```c
uint8_t packet_crc(const packet_t *pkt) {
    uint8_t buf[71];  // exclude crc byte
    buf[0] = pkt->devices;
    buf[1] = pkt->expecting_data;
    buf[2] = pkt->last_packet;
    buf[3] = (uint8_t)(pkt->w_value & 0xFF);
    buf[4] = (uint8_t)((pkt->w_value >> 8) & 0xFF);
    memcpy(&buf[5], pkt->data, 64);
    return crc8(buf, sizeof(buf));
}
```

**Important:** Set `pkt->crc = 0` before calculating, or exclude it from the buffer.

---

## Example: Sending RGB Color

```c
// Color packet
packet_t pkt = {0};
pkt.devices = 1;           // target device 1 (throttle)
pkt.w_value = 0x0309;
pkt.data[0] = 0x09;
pkt.data[1] = 0x00;
pkt.data[2] = 0x03;
pkt.data[3] = 255;         // R
pkt.data[4] = 0;           // G
pkt.data[5] = 128;         // B
pkt.crc = packet_crc(&pkt);

write(fd, &pkt, sizeof(pkt));

// End packet
packet_t end = {0};
end.devices = 1;
end.last_packet = 1;
end.w_value = 0x0300;
end.data[0] = 0x01;
end.data[1] = 0x01;
end.crc = packet_crc(&end);

write(fd, &end, sizeof(end));

// Read response
read(fd, &pkt, sizeof(pkt));
if (pkt.data[0] == 1) {
    // Success
}
```

---

## Example: Getting Axis Configuration

```c
packet_t pkt = {0};
pkt.devices = 1;           // device 1 (throttle)
pkt.expecting_data = 1;    // expect response
pkt.w_value = 0x0202;
pkt.data[0] = 0x02;
pkt.data[1] = 0x00;
pkt.data[2] = 0x0b;
pkt.data[3] = 30;          // axis X
pkt.crc = packet_crc(&pkt);

write(fd, &pkt, sizeof(pkt));
read(fd, &pkt, sizeof(pkt));

// Verify CRC
if (pkt.crc != packet_crc(&pkt)) {
    // CRC mismatch - handle error
}

// Parse response
uint8_t axis = pkt.data[4];
uint16_t xsat = (pkt.data[9] << 8) | pkt.data[10];
uint16_t ysat = (pkt.data[11] << 8) | pkt.data[12];
uint16_t deadband = (pkt.data[13] << 8) | pkt.data[14];
uint16_t curve = (pkt.data[15] << 8) | pkt.data[16];
```

---

## Error Response

If CRC verification fails, the daemon sends back:
```
devices: 0
data[0]: 0
crc: valid
```

The client should verify CRC on all responses.

---

## Calibration Process (Joystick)

1. Select X axis (packet with data[0]=0x0b, data[3]=30)
2. Send config packet with xsat, ysat, deadband, curve defaults, hall_call=0
3. Select Y axis (packet with data[0]=0x0b, data[3]=31)
4. Send config packet with same values
5. Select X axis again
6. Apply configuration packet
7. Select Y axis again
8. Apply configuration packet

**Note:** For hall effect calibration, center the joystick, sample X/Y positions, and write the averaged center value to hall_call (bytes 18-19).