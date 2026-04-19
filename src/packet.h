#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stddef.h>

#define MAX_DEVICES 8

typedef struct {
  uint8_t devices;        // bitmask: bit 0 = device 1, bit 1 = device 2, etc.
  uint8_t expecting_data; // client is expecting data back
  uint8_t last_packet;    // if its the last packet
//  uint8_t success;        // if command completed correctly or not
  uint16_t w_value;       // USB control transfer wValue
  uint8_t data[64];       // actual data
  uint8_t crc;            // CRC-8 checksum
} packet_t;

// CRC-8 (polynomial 0x07)
static inline uint8_t crc8(const uint8_t *data, size_t len) {
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

static inline uint8_t packet_crc(const packet_t *pkt) {return crc8((const uint8_t*)pkt, __builtin_offsetof(packet_t, crc));}

#endif
