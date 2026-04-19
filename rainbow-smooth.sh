#!/bin/bash

# Smooth rainbow gradient for X-56 controllers
# Usage: ./rainbow-smooth.sh [device] [speed] [brightness]
#   device: 1=throttle, 2=joystick, 0=both (default: 0)
#   speed: steps per color cycle (default: 100)
#   brightness: 0-100 (default: 50)

DEV=${1:-0}
SPEED=${2:-100}
BRIGHTNESS=${3:-50}

DIR="$(cd "$(dirname "$0")" && pwd)"
X56_CTRL="${DIR}/result/bin/x56-ctrl"

if [ ! -f "$X56_CTRL" ]; then
  echo "Building..."
  cd "$DIR/c" && nix build
fi

hsv_to_rgb() {
  local h=$1 s=$2 v=$3
  local i f
  
  if (( s == 0 )); then
    echo "$v,$v,$v"
    return
  fi
  
  i=$(( h / 60 ))
  f=$(( h % 60 ))
  
  # p = v * (1 - s)
  local p=$(( v * (100 - s) / 100 ))
  # q = v * (1 - s * f / 60)
  local q=$(( v * (100 - s * f / 60) / 100 ))
  # t = v * (1 - s * (60 - f) / 60)
  local t=$(( v * (100 - s * (60 - f) / 60) / 100 ))
  
  case $i in
    0) echo "$v,$t,$p" ;;
    1) echo "$q,$v,$p" ;;
    2) echo "$p,$v,$t" ;;
    3) echo "$p,$q,$v" ;;
    4) echo "$t,$p,$v" ;;
    5) echo "$v,$p,$q" ;;
  esac
}

echo "Smooth rainbow on device $DEV (Ctrl+C to stop)"
hue=0
while true; do
  rgb=$(hsv_to_rgb $((hue % 360)) 100 "$BRIGHTNESS")
  "$X56_CTRL" -d "$DEV" -r "$rgb" >/dev/null 2>&1
  hue=$((hue + 360 / SPEED))
  sleep 0.01
done
