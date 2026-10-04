#!/usr/bin/env bash
# Sign the FSBL and Appli images for flash boot and optionally write them to
# the external NOR flash of the STM32N6570-DK.
#
# Inputs:  FSBL/Release/mtk3bsp2_stm32n657_FSBL.bin   (Release: copies Appli from NOR)
#          Appli/Debug/mtk3bsp2_stm32n657_Appli.bin
# Outputs: build/flash/{FSBL-trusted.bin,Appli-trusted.bin,network_data.hex}
set -euo pipefail

usage() {
  cat >&2 <<EOF
usage: $0 [--flash] [--with-weights]

  --flash         write FSBL and Appli to the external NOR flash (board in Development mode)
  --with-weights  also write the model weights (network_data.hex), together with --flash

Set STM32_PROGRAMMER_BIN to override the STM32CubeProgrammer bin directory.
EOF
  exit 2
}

flash=0
with_weights=0
for arg in "$@"; do
  case "$arg" in
    --flash) flash=1 ;;
    --with-weights) with_weights=1 ;;
    *) usage ;;
  esac
done
if [[ $with_weights -eq 1 && $flash -eq 0 ]]; then
  usage
fi

project_root=$(git rev-parse --show-toplevel)
programmer_bin=${STM32_PROGRAMMER_BIN:-"/c/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin"}
signing_tool="$programmer_bin/STM32_SigningTool_CLI"
programmer="$programmer_bin/STM32_Programmer_CLI"
loader="$programmer_bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr"

fsbl_bin="$project_root/FSBL/Release/mtk3bsp2_stm32n657_FSBL.bin"
appli_bin="$project_root/Appli/Debug/mtk3bsp2_stm32n657_Appli.bin"
weights_hex="$project_root/Appli/FaceDetection/Model/network_data.hex"
out_dir="$project_root/build/flash"

# NOR addresses and size limits.  The Appli limit must match
# EXTMEM_LRUN_SOURCE_SIZE in FSBL/Core/Inc/stm32_extmem_conf.h, and the FSBL
# limit is the ROM region of FSBL/STM32N657X0HXQ_AXISRAM2_fsbl.ld plus the
# 0x400 byte header.
fsbl_address=0x70000000
appli_address=0x70100000
fsbl_max=$((0x400 + 255 * 1024))
appli_max=$((0x00100000))

for file in "$signing_tool.exe" "$fsbl_bin" "$appli_bin" "$weights_hex"; do
  if [[ ! -f "$file" ]]; then
    echo "missing: $file" >&2
    case "$file" in
      "$fsbl_bin") echo "build mtk3bsp2_stm32n657_FSBL with the Release configuration" >&2 ;;
      "$appli_bin") echo "build mtk3bsp2_stm32n657_Appli with the Debug configuration" >&2 ;;
    esac
    exit 1
  fi
done

mkdir -p "$out_dir"

sign() {
  local input=$1 output=$2
  # -nk: development header without keys.  -align: put the payload at offset
  # 0x400 so that the vector table lands on the address the image was linked at.
  "$signing_tool" -bin "$input" -nk -of 0x80000000 -t fsbl -hv 2.3 -align -s \
    -o "$output" >/dev/null
}

check_size() {
  local file=$1 limit=$2 size
  size=$(stat -c %s "$file")
  if (( size > limit )); then
    printf '%s is %d bytes, larger than the limit of %d bytes\n' "$file" "$size" "$limit" >&2
    exit 1
  fi
  printf '%-20s %8d / %8d bytes\n' "$(basename "$file")" "$size" "$limit"
}

sign "$fsbl_bin" "$out_dir/FSBL-trusted.bin"
sign "$appli_bin" "$out_dir/Appli-trusted.bin"
cp "$weights_hex" "$out_dir/network_data.hex"

check_size "$out_dir/FSBL-trusted.bin" "$fsbl_max"
check_size "$out_dir/Appli-trusted.bin" "$appli_max"
echo "images written to $out_dir"

if [[ $flash -eq 0 ]]; then
  exit 0
fi

writes=(-w "$out_dir/FSBL-trusted.bin" "$fsbl_address"
        -w "$out_dir/Appli-trusted.bin" "$appli_address")
if [[ $with_weights -eq 1 ]]; then
  writes+=(-w "$out_dir/network_data.hex")
fi

# Low SWD frequency and connect-under-reset: faster or hot-plug connections
# stalled ST-LINK USB during long external flash writes.
"$programmer" -c port=SWD freq=1000 mode=UR reset=HWrst -el "$loader" \
  "${writes[@]}" -hardRst
