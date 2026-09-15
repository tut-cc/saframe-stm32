#!/bin/sh
# Program the BlazeFace model weights into external XSPI2 flash.
# Only needed once per board: the build and the debug launch do not do it.
# face_detection_app.c checks the first three words at NETWORK_WEIGHTS_ADDRESS
# (0x70380000) and disables inference if they do not match.
#
# Run with no debug client attached (no gdbserver, no CubeIDE/VSCode session),
# or STM32_Programmer_CLI fails with a connect error.
set -eu
CLT=${STM32CUBECLT:-/opt/ST/STM32CubeCLT_1.22.0}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
LOADER=${STM32N6_LOADER:-$CLT/STM32CubeProgrammer/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr}
HEX=$ROOT/Appli/FaceDetection/Model/network_data.hex
[ -f "$LOADER" ] || { echo "external loader not found: $LOADER" >&2; exit 1; }
[ -f "$HEX" ] || { echo "model weights not found: $HEX" >&2; exit 1; }
# The .hex carries its own load addresses (0x70380000..0x7039A7E7), so -w alone.
exec "$CLT/STM32CubeProgrammer/bin/STM32_Programmer_CLI" \
  -c port=SWD mode=HOTPLUG freq=1000 -el "$LOADER" -hardRst -w "$HEX"
