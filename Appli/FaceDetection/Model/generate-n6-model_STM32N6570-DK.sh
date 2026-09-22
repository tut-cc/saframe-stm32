#!/bin/bash
#
# Regenerate the Neural-ART model artifacts (network.c, network_data.hex, ...)
# for STM32N6570-DK from a quantized model, using the local stedgeai toolchain.
#
# Adapted from STMicroelectronics/STM32N6-GettingStarted-FaceDetection's
# Model/generate-n6-model_STM32N6570-DK.sh: same stedgeai invocation and the
# same board neural-art config (user_neuralart_STM32N6570-DK.json,
# my_mpools/stm32n6-app2_STM32N6570-DK.mpool, both vendored alongside this
# script), but copies straight into this repo's flat Appli/FaceDetection/Model/
# layout instead of a per-board subdirectory.
#
set -eu # Exit on any error, exit on unset variable

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <quantized model.tflite or .onnx>" >&2
  exit 2
fi

model_path=$1
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$script_dir"

stedgeai generate --model "$model_path" --target stm32n6 \
  --st-neural-art default@user_neuralart_STM32N6570-DK.json \
  --input-data-type uint8 --output-data-type int8

cp st_ai_output/network.c "$script_dir/"
cp st_ai_output/network_ecblobs.h "$script_dir/"
cp st_ai_output/stai_network.c "$script_dir/"
cp st_ai_output/stai_network.h "$script_dir/"
cp st_ai_output/network_atonbuf.xSPI2.raw "$script_dir/network_data.xSPI2.bin"
arm-none-eabi-objcopy -I binary "$script_dir/network_data.xSPI2.bin" \
  --change-addresses 0x70380000 -O ihex "$script_dir/network_data.hex"

echo "Generated Neural-ART artifacts in $script_dir"
echo "Review: git diff -- Appli/FaceDetection/Model"
echo "Also update app_config.h (AI_FD_YUNET_PP_* / anchors) and the"
echo "NetworkWeightsValid() signature table in face_detection_app.c."
