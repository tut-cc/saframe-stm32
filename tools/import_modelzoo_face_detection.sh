#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <stm32ai-modelzoo-services/application_code/face_detection/STM32N6>" >&2
  exit 2
fi

source_root=${1%/}
model_source="$source_root/Model/STM32N6570-DK"
inc_source="$source_root/Application/STM32N6570-DK/Inc"
config_source="$inc_source/app_config.h"
project_root=$(git rev-parse --show-toplevel)
model_target="$project_root/Appli/FaceDetection/Model"
config_target="$project_root/Appli/FaceDetection/Inc/app_config.h"
postprocess_inc_target="$project_root/Appli/FaceDetection/Vendor/Postprocess/Inc"

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

anchors=(fd_yunet_anchors_32.h fd_yunet_anchors_16.h fd_yunet_anchors_8.h)
for file in "${anchors[@]}"; do
  if [[ ! -f "$inc_source/$file" ]]; then
    echo "missing generated file: $inc_source/$file" >&2
    exit 1
  fi
done

for file in "${required[@]}"; do
  cp "$model_source/$file" "$model_target/$file"
done
cp "$config_source" "$config_target"
for file in "${anchors[@]}"; do
  cp "$inc_source/$file" "$postprocess_inc_target/$file"
done

echo "Imported Model Zoo face-detection (YuNet) config, anchors and Neural-ART artifacts."
echo "Review: git diff -- Appli/FaceDetection/Inc/app_config.h Appli/FaceDetection/Model Appli/FaceDetection/Vendor/Postprocess/Inc"
echo "Also update the NetworkWeightsValid() signature table in Appli/FaceDetection/Src/face_detection_app.c."
