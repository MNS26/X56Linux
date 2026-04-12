# <b>!!!THIS PROJECT IS NOT FULLY COMPLETE AND IS A ONGOING!!!</b>

# X-56 HOTAS on Linux
fist things first... this project is based on an existing project by `Chryseus` [x56linux](https://github.com/Chryseus/x56linux) parts of the code used come from there. he has done an amazing job finding the usb packets. 

## Why This Project?

The X-56 HOTAS (Hands On Throttle And Stick) is a great fliht-sim controller for mid-range setups (came from a x52 before). On Linux, there is no driver or software for configuring or changing lights or settings. Chryseus tool works as well but takes over the HOTAS and can't be used in games afterwards. This project solves that by:

1. **Only claiming interface 2** (the RGB/configuration HID interface) - leaving interfaces 0 and 1 for standard game input
2. **Running as a seperate daemon** - accepting commands via Unix socket allowing non-sudo usage for interacting with the hardware


## Building

This project uses `nix` for building.(nix just calls the compiler directly as well).<br>
It also makes it much easier to build cross-platfroms.
```bash
nix build
```

Or for development:

```bash
nix develop
gcc -o x56d -Wall -std=c99 src/main.c src/usb.c $(pkg-config --cflags --libs libusb-1.0)
gcc -o x56-ctrl -Wall -std=c99 src/x56-ctrl.c
```

## Running

### Manual way

```bash
./x56d  # Run in seperate terminal or use -d to run in background
./x56-ctrl -d 1 -r 255,0,0  # Set throttle to red
./x56-ctrl -d 2 -r 0,255,0  # Set joystick to green
./x56-ctrl -d 3 -r 0,0,255  # Set both to blue
```

### With Systemd
After building copy the service file and enable:

```bash
cp result/systemd/system/x56d.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable x56d
systemctl start x56d
```

### With udev
With udev it can automatically turn off the lights when plugged in.

```bash
cp result/udev/rules.d/99-x56.rules /etc/udev/rules.d/
udevadm control --reload-rules
udevadm trigger
```


## Usage
### What works and what does not
 [x] RGB <br>
 [ ] Calibrating <br>
 [ ] Curves <br>
 [ ] Deadzones <br>
 [ ] Axis selection <br>


### x56-ctrl Command Line Options

| Option | Description |
|--------|-------------|
| `-d ID` | Target device (1=throttle, 2=joystick, 3=both) |
| `-r R,G,B` | Set RGB color (e.g., `-r 255,128,0`) |
| `-a ID` | Axis ID for configuration (30=X, 31=Y, etc.) |
| `--get` | Get current axis configuration |

### Examples

```bash
# Set throttle RGB to red
x56-ctrl -d 1 -r 255,0,0

# Set joystick RGB to cyan
x56-ctrl -d 2 -r 0,255,255

# Set both devices to purple
x56-ctrl -d 3 -r 128,0,128

# Get throttle X axis configuration
x56-ctrl -d 1 -a 30 --get
```

## Documentation

See the [doc/](doc/) folder for detailed protocol documentation:

- [doc/Packets.md](doc/Packets.md) - Packet format and socket protocol

## Architecture

- **Daemon (x56d)**: Uses libusb hotplug callbacks to detect device connect/disconnect, Usees HID interface 2, listens on Unix socket in `/run/x56-daemon.sock`
- **Control tool (x56-ctrl)**: Sends commands to daemon via Unix socket
- **Protocol**: CRC-8 verified packets (to prevent rouge data or accidental data)

## Files

```
X56Linux
├── src/
│   ├── main.c        # Daemon implementation
│   ├── usb.c         # USB/hotplug handling
│   ├── usb.h         # USB structures
│   ├── x56-ctrl.c    # Control tool
│   ├── x56-ctrl.h    # Control tool header
│   └── packet.h      # Packet structure with CRC
├── systemd/
│   └── x56d.service  # SystemD service
├── udev/
│   └── 99-x56.rules  # Udev rule
├── doc/
│   └── Packets.md    # Protocol documentation
├── rainbow.sh        # first rought colors stepping test. left in because why not
├── rainbow-smooth.sh # actual rainbow. left in cus well... ITS A RAINBOW
└── flake.nix
```

## License

TBD