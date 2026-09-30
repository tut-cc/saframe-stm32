#!/usr/bin/env bash
set -euo pipefail

contract=
if [[ ${1:-} == "--contract" ]]; then
  contract=${2:-}
  shift 2
fi

if [[ $# -ne 1 ]]; then
  echo "usage: $0 [--contract <model-contract.json>] <stm32ai-modelzoo-services/application_code/object_detection/STM32N6>" >&2
  exit 2
fi

source_root=${1%/}
model_source="$source_root/Model/STM32N6570-DK"
config_source="$source_root/Application/STM32N6570-DK/Inc/app_config.h"
project_root=$(git rev-parse --show-toplevel)
contract=${contract:-"$project_root/modelzoo/st_yoloxn_person_320.json"}
model_target="$project_root/Appli/FaceDetection/Model"
config_target="$project_root/Appli/FaceDetection/Inc/app_config.h"
signature_target="$project_root/Appli/FaceDetection/Inc/model_signature.h"

required=(network.c network_data.hex network_data.xSPI2.bin network_ecblobs.h stai_network.c stai_network.h)
for file in "${required[@]}"; do
  if [[ ! -f "$model_source/$file" ]]; then
    echo "missing generated file: $model_source/$file" >&2
    exit 1
  fi
done
if [[ ! -f "$config_source" ]]; then
  echo "missing generated file: $config_source" >&2
  exit 1
fi

python3 "$project_root/tools/verify_model_artifacts.py" \
  "$model_source" "$config_source" "$contract" \
  --edgeai-dir "$project_root/Appli/FaceDetection/Vendor/EdgeAI" \
  --library-dir "$project_root/Appli/FaceDetection/Vendor/Lib"

signature_tmp=$(mktemp)
trap 'rm -f "$signature_tmp"' EXIT
python3 "$project_root/tools/generate_model_signature.py" \
  "$model_source/network_data.hex" "$signature_tmp"

for file in "${required[@]}"; do
  cp "$model_source/$file" "$model_target/$file"
done
cp "$config_source" "$config_target"
cp "$signature_tmp" "$signature_target"

echo "Imported Model Zoo object-detection config and Neural-ART artifacts."
echo "Imported contract: $contract"
echo "Review: git diff -- Appli/FaceDetection/Inc Appli/FaceDetection/Model"
