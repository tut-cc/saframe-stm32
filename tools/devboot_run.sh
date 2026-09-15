#!/bin/sh
# Dev-boot run from the command line (verified on STM32N6570-DK, macOS, 2026-09-11).
#   1. ST-LINK_gdbserver at 1 MHz SWD (faster settings stall the ST-LINK V3 on macOS)
#   2. gdb loads Appli.elf then FSBL.elf in 1 KiB packets and halts at FSBL main
#   3. only then the VCP reader opens: reading the VCP while gdb transfers
#      degrades the SWD link on the same USB device
#   4. run to usermain, detach, stop the server, let the reader collect output
#   Model weights must already be in XSPI2 flash; run FLASH_MODEL=1 once (or
#   tools/flash_model.sh) or the app prints "ERROR: program network_data.hex".
# Usage: FLASH_MODEL=1 tools/devboot_run.sh [SECONDS] [OUTFILE]   (any cwd; logs go to build/Debug/)
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
cd "$ROOT"
CLT=${STM32CUBECLT:-/opt/ST/STM32CubeCLT_1.22.0}
BUILD=${BUILD_DIR:-build/Debug}
SECS=${1:-30}
OUT=${2:-$BUILD/vcp.log}
VCP=${VCP:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}
[ -n "$VCP" ] || { echo "no /dev/cu.usbmodem* found" >&2; exit 1; }
APPLI=$BUILD/mtk3bsp2_stm32n657_Appli.elf
FSBL=$BUILD/mtk3bsp2_stm32n657_FSBL.elf
if [ "${FLASH_MODEL:-0}" = "1" ]; then "$SCRIPT_DIR/flash_model.sh"; fi
# One debug client at a time: stop any VSCode / CubeIDE debug session first, or the
# gdbserver fails with "Device connect error" / "Target USB comms error".
"$CLT/STLink-gdb-server/bin/ST-LINK_gdbserver" -e -k -d -m 1 -p 61234 --frequency 1000 \
  -cp "$CLT/STM32CubeProgrammer/bin" > "$BUILD/gdbserver.log" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null' EXIT
sleep 5
if grep -qi error "$BUILD/gdbserver.log"; then cat "$BUILD/gdbserver.log" >&2; echo "gdbserver failed (re-plug the ST-LINK USB after a stall)" >&2; exit 1; fi
"$CLT/GNU-tools-for-STM32/bin/arm-none-eabi-gdb" -batch -nx "$FSBL" \
  -ex 'set pagination off' -ex 'set confirm off' \
  -ex 'set remote memory-write-packet-size 1024' -ex 'set remote memory-write-packet-size fixed' \
  -ex 'target extended-remote :61234' -ex 'monitor reset' \
  -ex "load $APPLI" -ex "load $FSBL" -ex "add-symbol-file $APPLI" \
  -ex 'tbreak main' -ex 'continue' \
  -ex "shell (python3 \"$SCRIPT_DIR/vcp_read.py\" $VCP $SECS $OUT > $BUILD/vcp_read.status 2>&1 &); sleep 2" \
  -ex 'tbreak usermain' -ex 'continue' -ex 'detach'
kill $SRV 2>/dev/null; trap - EXIT
sleep "$((SECS + 3))"
cat "$BUILD/vcp_read.status"
cat "$OUT"
