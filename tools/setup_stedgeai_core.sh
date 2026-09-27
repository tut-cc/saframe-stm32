#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <STM32CubeIDE download-cache> <destination>" >&2
  exit 2
fi

cache_root=$1
destination=$2

find_one() {
  local pattern=$1
  local matches=()
  mapfile -t matches < <(find "$cache_root" -type f -name "$pattern" -print)
  if [[ ${#matches[@]} -ne 1 ]]; then
    echo "expected exactly one $pattern below $cache_root; found ${#matches[@]}" >&2
    return 1
  fi
  printf '%s\n' "${matches[0]}"
}

base_zip=$(find_one '4.0.1.*stedgeai-base-linux-4.0.1.zip')
core_zip=$(find_one '4.0.1.*stedgeai-core-linux-4.0.1.zip')
mcu_zip=$(find_one '12.0.1.*stedgeai-stm32mcu-linux-12.0.1.zip')
neural_art_zip=$(find_one '12.0.1.*stedgeai-stneuralart-12.0.1.zip')

mkdir -p "$destination"
for archive in "$base_zip" "$core_zip" "$mcu_zip" "$neural_art_zip"; do
  unzip -n -q "$archive" -d "$destination"
done

stedgeai="$destination/Utilities/linux/stedgeai"
if [[ ! -x "$stedgeai" ]]; then
  echo "STEdgeAI executable was not installed: $stedgeai" >&2
  exit 1
fi

"$stedgeai" --version
printf 'STEDGEAI_PATH=%s\n' "$stedgeai"
