(should be pretty much the same as the joystick... have yet to actually check though)

### Axis ID's
Throttle1 = 30
Throttle2 = 31
Rotary1 = 32
(missing 33, 34 ???)
Rotary2 = 35
Rotary3 = 37
Rotary4 = 36

---

### Axis Selection (Calibration & curve)
packet 1 (Unknown, To Be Named)
| byte | Value | Desc |
|:----:|:-----:|:----:|
| `0`  |  0x0b |      |
| `1`  |  0x03 |      |
| `2`  |  0x00 |      |
| `3`  |  0xXX | AXIS |

Packet 2 (Unknown, To Be Named)
| byte | Value | Desc |
|:----:|:-----:|:----:|
| `0`  |  0x0b |      |
| `1`  |  0x04 |      |
| `2`  |  0x00 |      |
| `3`  |  0xXX | AXIS |

---

### Setting RGB
packet 1 (Set Color)
| byte | Value | Desc |
|:----:|:-----:|:----:|
| `0`  |  0x09 |      |
| `1`  |  0x00 |      |
| `2`  |  0x03 |      |
| `3`  | 0-255 |  RED |
| `4`  | 0-255 |GREEN |
| `5`  | 0-255 | BLUE |

---

### Save RGB color (so far only seen for RGB colors) 
Packet 1 (Save RGB color)
| byte | Value | Desc |
|:----:|:-----:|:----:|
| `0`  |  0x01 |      |
| `1`  |  0x01 |      |

---

### Calibration sequence for X or Y axis joystick hall sensor
(Axis Select packets)

Packet 3 (set defaults)
|  byte  | Value  | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0x0b  |       |
|  `1`   |  0x01  |       |
|  `2`   |  0x00  |       |
|  `3`   |  0xXX  | AXIS  |
|  `4`   |  0x00  |       |
|  `5`   |  0x00  |       |
|  `6`   |  0x00  |       |
|  `7`   |  0x00  |       |
| `8-9`  | 0x03e8 | XSAT  | (1000 is default)
|`10-11` | 0x03e8 | YSAT  | (1000 is default)
|`12-13` | 0x0000 | DBAND | (1000 is default)
|`14-15` | 0x01f4 | CURVE | (500 is default)
|  `16`  |  0x00  |PROFILE| (00 is default)
|`17-18` | 0x0000 | VALUE | (0000 for calibration)

(Sample axis here and average out)

Packet 4 (set calibration)
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0x0b  |       |
|  `1`   |  0x01  |       |
|  `2`   |  0x00  |       |
|  `3`   |  0xXX  | AXIS  |
|  `4`   |  0x00  |       |
|  `5`   |  0x01  |       |
|  `6`   |  0x00  |       |
|  `7`   |  0xXX  | AXIS  |
| `8-9`  | 0xXXXX | XSAT  | (1000 is default)
|`10-11` | 0xXXXX | YSAT  | (1000 is default)
|`12-13` | 0xXXXX | DBAND | (1000 is default)
|`14-15` | 0xXXXX | CURVE | (500 is default)
|  `16`  |  0xXX  |PROFILE| (00 = S, 01 = J)
|`17-18` | 0xXXXX | VALUE | (raw value read from axis)

---

## select axis for config request 
Packet 1 (Unknown) [Set Report] [wValue 0x030b]
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0x0b  |       |
|  `1`   |  0x02  |       |
|  `2`   |  0x00  |       |
|  `3`   |  0x30  |AXIS ID|
(should be followed by a get_report)

---

### Get Axis Packet (doesnt look like anythign usefull)

(select axis for config request)
Packet ??? (get Axis config) [Get Report] [wValue 0x0308]
(get_report response example)
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
| `0-16` |  0x30  | AXIS  | (axis value repeates)
|  `17`  |  0x02  | ????? |
|  `18`  |  0x3f  | ????? |
|`19-41` |  0x00  |       |
|  `42`  |  0x30  |       |
|  `43`  |  0x30  |       |
|  `44`  |  0x59  |       |
|  `45`  |  0xa6  |       |
|  `46`  |  0x12  |       |
|  `47`  |  0x03  |       |
|  `48`  |  0x30  |       |
|  `49`  |  0x30  |       |
|  `50`  |  0x30  |       |
|  `51`  |  0x30  |       |
|  `52`  |  0x30  |       |
|  `53`  |  0x30  |       |
|  `54`  |  0x30  |       |
|  `55`  |  0x30  |       |
|  `56`  |  0x30  |       |
|  `57`  |  0x30  |       |
|  `58`  |  0x30  |       |
|  `59`  |  0x30  |       |
|  `60`  |  0x30  |       |
|  `61`  |  0x30  |       |
|  `62`  |  0x30  |       |
|  `64`  |  0x55  |       |

---

### Get specific axis config
(select axis config request)
send interupt read to interface 2 to read axis config. this returns
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0xff  |       |
|  `1`   |  0x0b  |       | (seems to be our command)
|  `2`   |  0x02  |       | (seems to be our command)
|  `3`   |  0x00  |       | (seems to be our command)
|  `4`   |  0xXX  | AXIS  | (seems to be our command)
|  `5`   |  0x00  |       |
|  `6`   |  0x01  |       |
|  `7`   |  0x00  |       |
|  `8`   |  0xXX  | AXIS  |
| `9-10` | 0x03e8 | XSAT  |
|`11-12` | 0x03e8 | YSAT  |
|`13-14` | 0x0000 | DBAND |
|`15-16` | 0x01f4 | CURVE |
|  `17`  |  0x00  |PROFILE|
|`18-19` | 0x7bf4 |  CAL  | (zero for non hall effect sensor)

---

### Unknown packet
Packet ??? (Unknown) [Set Report] [wValue 0x0309]
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0x09  |       |
|  `1`   |  0x02  |       |
|  `2`   |  0x02  | ????? | (so far starts at 0x02 and seen up to 0x03) 

---

### Unknown packet (could be read interrup data size)
Packet ??? (Unknown) [Set Report] [wValue 0x0309]
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0x09  |       |
|  `1`   |  0x02  |       |
|  `2`   |  0x02  |       |

followed by a interrupt read on endpoint 2. that returns 
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0xff  |       |
|  `1`   |  0x09  |       |
|  `2`   |  0x02  |       |
|  `3`   |  0x02  |       |
|  `4`   |  0x3f  |       | 

---

### Unknown packet (could be curve array size)
Packet ??? (Unknown) [Set Report] [wValue 0x0309]
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0x09  |       |
|  `1`   |  0x02  |       |
|  `2`   |  0x03  |       |

followed by a interrupt read on endpoint 2. that returns 
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0xff  |       |
|  `1`   |  0x09  |       |
|  `2`   |  0x02  |       |
|  `3`   |  0x00  |       |
|  `4`   |  0x0b  |       | 
|  `5`   |  0x55  |       | 
|  `6`   |  0xd6  |       | 

---

### Update curve
(select specific axis )
Throttle1 = 10bit value 
Throttle2 = 10bit 
Rotary1 = 8bit
Rotary2 = 8bit
Rotary3 = 8bit
Rotary4 = 8bit

send curve as an array of raw values.<br>
final entry must be FF (so 03FF for 10bit or FF for 8bit).<br>
packets are sent over bulk interface. <br>
```
10bit
1024 / 64 * 2(16 -> 8bits) = 32 packets

8bit
256 / 64 = 4 packets 
```
(example 8bit & 10bit)
```c
//8bit
cuve[0] = 0x00;
cuve[1] = 0x01;
/*...*/
cuve[254] = 0xfd;
cuve[255] = 0xff;

//10bit
cuve[0] = 0x0000 >> 8;
cuve[1] = 0x0000 &  0xff;
cuve[2] = 0x0001 >> 8;
cuve[2] = 0x0001 &  0xff;
/*...*/
cuve[1020] = 0xfffd >> 8;
cuve[1021] = 0xfffd & 0xff;
cuve[1022] = 0xfffe >> 8;
cuve[1023] = 0xff;
```
---
