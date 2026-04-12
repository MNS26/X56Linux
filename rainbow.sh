#!/bin/bash

# Rainbow effect for X-56 controllers
# Usage: ./rainbow.sh [device] [speed]
#   device: 1=throttle, 2=joystick, 3=both (default: 3)
#   speed: delay in seconds (default: 0.1)

DEV=${1:-3}
SPEED=${2:-0.1}

# Rainbow colors (R,G,B)
COLORS=(
  "255,0,0"     # Red
  "255,127,0"   # Orange
  "255,255,0"   # Yellow
  "0,255,0"     # Green
  "0,0,255"     # Blue
  "75,0,130"    # Indigo
  "238,130,238" # Violet
)

DIR="$(dirname "$0")"
X56_CTRL="${DIR}/result/bin/x56-ctrl"
echo "tool dir ${X56_CTRL}"

if [ ! -f "$X56_CTRL" ]; then
  echo "Building..."
  cd "$DIR/c" && nix build
  X56_CTRL="$DIR/c/result/bin/x56-ctrl"
fi

echo "Running rainbow on device $DEV (Ctrl+C to stop)"
while true; do
  for color in "${COLORS[@]}"; do
    "$X56_CTRL" -d "$DEV" -r "$color" >/dev/null 2>&1
    sleep "$SPEED"
  done
done