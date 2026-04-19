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

### Unknown (possible enf of config) 
Packet 2 (unknown what this does)
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

### Get Axis Packet

(select axis for config request)
Packet ??? (get Axis config) [Get Report] [wValue 0x0308]
(get_report response example)
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
| `0-16` |  0x30  | AXIS  | (axis value repeates)
|  `17`  |  0x01  | ????? |
|  `18`  |  0x3f  | ????? |
|  `19`  |  0x0b  |       |-\
|  `20`  |  0x02  |       |--\ (looks like axis selection packet)
|  `21`  |  0x00  |       |--/
|  `22`  |  0x30  |       |-/
|  `23`  |  0x00  | ????? |
|  `24`  |  0x01  | ????? |
|  `25`  |  0x00  | ????? |
|  `26`  |  0x30  | AXIS? |
|`27-28` | 0x03e8 | XSAT  |
|`29-30` | 0x03e8 | YSAT  |
|`31-32` | 0x0000 | DBAND |
|`33-34` | 0x01f4 | CURVE |
|  `35`  |  0x00  |PROFILE|
|`36-37` | 0x7bf4 |  CAL  | (zero for non hall effect sensors)

---

### Unknown packet
Packet ??? (Unknown) [Set Report] [wValue 0x0309]
|  byte  |  Value | Desc  |
|:------:|:------:|:-----:|
|  `0`   |  0x09  |       |
|  `1`   |  0x02  |       |
|  `2`   |  0x02  | ????? | (so far starts at 0x02 and seen up to 0x03) 

---

### Update curve
(select axis )(only saw packet 1 though and not 2)
send 2047 backets over bulk interface
65535 * 2 = 131070 / 64 = 2047 packets
curve is sent as a range from 0x0000 to 0xFFFF
