#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <stm32ai-modelzoo-services/application_code/object_detection/STM32N6>" >&2
  exit 2
fi

source_root=${1%/}
model_source="$source_root/Model/STM32N6570-DK"
config_source="$source_root/Application/STM32N6570-DK/Inc/app_config.h"
project_root=$(git rev-parse --show-toplevel)
model_target="$project_root/Appli/FaceDetection/Model"
config_target="$project_root/Appli/FaceDetection/Inc/app_config.h"

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

for file in "${required[@]}"; do
  cp "$model_source/$file" "$model_target/$file"
done
cp "$config_source" "$config_target"

echo "Imported Model Zoo object-detection config and Neural-ART artifacts."
echo "Review: git diff -- Appli/FaceDetection/Inc/app_config.h Appli/FaceDetection/Model"
