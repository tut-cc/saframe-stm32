#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <STEdgeAI/Middlewares/ST/AI>" >&2
  exit 2
fi

ai_root=${1%/}
project_root=$(git rev-parse --show-toplevel)
edgeai_target="$project_root/Appli/FaceDetection/Vendor/EdgeAI"
library_target="$project_root/Appli/FaceDetection/Vendor/Lib"

required=(
  "$ai_root/Inc/stai.h"
  "$ai_root/Inc/core_datatypes.h"
  "$ai_root/Npu/ll_aton/ll_aton_version.h"
  "$ai_root/Lib/GCC/ARMCortexM55/NetworkRuntime1201_CM55_GCC.a"
)
for file in "${required[@]}"; do
  if [[ ! -f "$file" ]]; then
    echo "missing STEdgeAI runtime file: $file" >&2
    exit 1
  fi
done

# Keep the checked-in runtime closed over one STEdgeAI release.  Copy every
# already tracked runtime file instead of selecting only the version headers.
while IFS= read -r target; do
  relative=${target#Appli/FaceDetection/Vendor/EdgeAI/}
  case "$relative" in
    Inc/*) source="$ai_root/$relative" ;;
    ll_aton/*|Devices/*) source="$ai_root/Npu/$relative" ;;
    *) echo "unsupported EdgeAI path: $relative" >&2; exit 1 ;;
  esac
  if [[ ! -f "$source" ]]; then
    echo "runtime source is missing tracked file: $source" >&2
    exit 1
  fi
  cp "$source" "$edgeai_target/$relative"
  sed -i 's/\r$//' "$edgeai_target/$relative"
done < <(git -C "$project_root" ls-files 'Appli/FaceDetection/Vendor/EdgeAI/**')

cp "$ai_root/Lib/GCC/ARMCortexM55/NetworkRuntime1201_CM55_GCC.a" \
  "$library_target/NetworkRuntime1201_CM55_GCC.a"

echo "Synchronized complete STEdgeAI 4.0.1 / runtime 12.0.1 source set."
