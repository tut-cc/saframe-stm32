#!/usr/bin/env bash
set -euo pipefail

MODELZOO_COMMIT=0f6210ed5156126b782e1c43249063a477484b20
TARGET_EPOCHS=500
WORK_ROOT=${SAFRAME_RUNPOD_ROOT:-/workspace/saframe-proxy3}
REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODELZOO_ROOT="$WORK_ROOT/stm32ai-modelzoo-services"
VENV_ROOT="$WORK_ROOT/python311"
PYTHON_BIN="$VENV_ROOT/bin/python"
COCO_ROOT="$WORK_ROOT/coco"
DOWNLOAD_ROOT="$WORK_ROOT/downloads"
DATASET_ROOT="$WORK_ROOT/dataset"
EXPERIMENT_ROOT="$WORK_ROOT/experiments"
CONFIG="$REPO_ROOT/modelzoo/training_coco_proxy_3class.yaml"

usage() {
  echo "usage: $0 {setup|prepare|smoke|train|resume|all|status}" >&2
  exit 2
}

require_workspace() {
  if [[ ! -d /workspace ]]; then
    echo "RunPod persistent volume is not mounted at /workspace" >&2
    exit 1
  fi
}

require_initial_capacity() {
  if [[ -f "$COCO_ROOT/.train2017.zip.extracted" ]]; then
    return
  fi
  local free_kib
  free_kib=$(df --output=avail /workspace | tail -1)
  if (( free_kib < 65 * 1024 * 1024 )); then
    echo "At least 65 GiB free space is required before the first COCO preparation" >&2
    exit 1
  fi
}

install_uv() {
  if command -v uv >/dev/null 2>&1; then
    command -v uv
    return
  fi
  curl --fail --location --proto '=https' --tlsv1.2 https://astral.sh/uv/install.sh | sh >&2
  if [[ -x /root/.local/bin/uv ]]; then
    printf '%s\n' /root/.local/bin/uv
  else
    echo "uv installation did not create /root/.local/bin/uv" >&2
    exit 1
  fi
}

ensure_host_tools() {
  local missing=()
  local tool
  for tool in curl git unzip; do
    command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
  done
  if (( ${#missing[@]} > 0 )); then
    if ! command -v apt-get >/dev/null 2>&1; then
      echo "Missing host tools and apt-get is unavailable: ${missing[*]}" >&2
      exit 1
    fi
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y ca-certificates curl git unzip
  fi
}

setup_environment() {
  require_workspace
  ensure_host_tools
  command -v nvidia-smi >/dev/null
  nvidia-smi
  local gpu_memory_mib
  gpu_memory_mib=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits | head -1 | tr -d ' ')
  if (( gpu_memory_mib < 15000 )); then
    echo "At least 16 GB GPU memory is required; detected ${gpu_memory_mib} MiB" >&2
    exit 1
  fi
  mkdir -p "$WORK_ROOT" "$DOWNLOAD_ROOT" "$EXPERIMENT_ROOT"
  local uv_bin
  uv_bin=$(install_uv)
  if [[ ! -d "$MODELZOO_ROOT/.git" ]]; then
    git clone https://github.com/STMicroelectronics/stm32ai-modelzoo-services.git "$MODELZOO_ROOT"
  fi
  git -C "$MODELZOO_ROOT" fetch --depth=1 origin "$MODELZOO_COMMIT"
  git -C "$MODELZOO_ROOT" checkout --detach "$MODELZOO_COMMIT"
  "$uv_bin" python install 3.11
  if [[ ! -x "$PYTHON_BIN" ]]; then
    "$uv_bin" venv --python 3.11 "$VENV_ROOT"
  fi
  "$uv_bin" pip install --python "$PYTHON_BIN" -r "$MODELZOO_ROOT/requirements.txt"
  MPLBACKEND=Agg "$PYTHON_BIN" -c \
    'import tensorflow as tf; print(tf.__version__); print(tf.config.list_physical_devices("GPU")); assert tf.config.list_physical_devices("GPU")'
}

download_zip() {
  local filename=$1
  local url=$2
  local archive="$DOWNLOAD_ROOT/$filename"
  local partial="$archive.part"
  if [[ -f "$archive" ]] && ! unzip -Z -t "$archive" >/dev/null 2>&1; then
    echo "Removing invalid archive: $archive" >&2
    unlink "$archive"
  fi
  if [[ ! -f "$archive" ]]; then
    unlink "$partial" 2>/dev/null || true
    curl --fail --location --retry 5 --retry-all-errors --output "$partial" "$url"
    unzip -Z -t "$partial" >/dev/null
    mv "$partial" "$archive"
  fi
  printf '%s\n' "$archive"
}

extract_once() {
  local archive=$1
  local marker="$COCO_ROOT/.${archive##*/}.extracted"
  if [[ ! -f "$marker" ]]; then
    mkdir -p "$COCO_ROOT"
    unzip -q -o "$archive" -d "$COCO_ROOT"
    touch "$marker"
  fi
}

prepare_dataset() {
  require_workspace
  require_initial_capacity
  ensure_host_tools
  mkdir -p "$DOWNLOAD_ROOT" "$DATASET_ROOT"
  local train_zip val_zip annotations_zip
  train_zip=$(download_zip train2017.zip https://images.cocodataset.org/zips/train2017.zip)
  val_zip=$(download_zip val2017.zip https://images.cocodataset.org/zips/val2017.zip)
  annotations_zip=$(download_zip annotations_trainval2017.zip https://images.cocodataset.org/annotations/annotations_trainval2017.zip)
  extract_once "$train_zip"
  extract_once "$val_zip"
  extract_once "$annotations_zip"
  python3 "$REPO_ROOT/tools/extract_coco_proxy_classes.py" \
    "$COCO_ROOT/annotations/instances_train2017.json" \
    "$DATASET_ROOT/instances_train2017_proxy3.json" \
    --images "$COCO_ROOT/train2017" --darknet-output "$DATASET_ROOT/train"
  python3 "$REPO_ROOT/tools/extract_coco_proxy_classes.py" \
    "$COCO_ROOT/annotations/instances_val2017.json" \
    "$DATASET_ROOT/instances_val2017_proxy3.json" \
    --images "$COCO_ROOT/val2017" --darknet-output "$DATASET_ROOT/val"
  python3 "$REPO_ROOT/tools/validate_proxy3_dataset.py" "$DATASET_ROOT/train" \
    --summary "$DATASET_ROOT/train_summary.json"
  python3 "$REPO_ROOT/tools/validate_proxy3_dataset.py" "$DATASET_ROOT/val" \
    --summary "$DATASET_ROOT/val_summary.json"
}

run_chain() {
  local output=$1
  local epochs=$2
  local train_dir=$3
  local val_dir=$4
  shift 4
  mkdir -p "$output"
  cp "$CONFIG" "$output/effective_config.yaml"
  PROXY3_TRAIN_DIR="$train_dir" \
  PROXY3_VAL_DIR="$val_dir" \
  STEDGEAI_PATH=/bin/false \
  CUBEIDE_PATH=/bin/false \
  MPLBACKEND=Agg \
  "$PYTHON_BIN" "$MODELZOO_ROOT/object_detection/stm32ai_main.py" \
    --config-path "$REPO_ROOT/modelzoo" \
    --config-name training_coco_proxy_3class.yaml \
    operation_mode=chain_tqe \
    "training.epochs=$epochs" \
    "hydra.run.dir=$output" \
    "$@" 2>&1 | tee -a "$output/run.log"
}

run_smoke() {
  python3 "$REPO_ROOT/tools/make_proxy3_subset.py" "$DATASET_ROOT/train" \
    "$DATASET_ROOT/smoke_train" --limit 256
  python3 "$REPO_ROOT/tools/make_proxy3_subset.py" "$DATASET_ROOT/val" \
    "$DATASET_ROOT/smoke_val" --limit 64
  run_chain "$EXPERIMENT_ROOT/smoke" 1 \
    "$DATASET_ROOT/smoke_train" "$DATASET_ROOT/smoke_val"
}

run_full() {
  run_chain "$EXPERIMENT_ROOT/full" "$TARGET_EPOCHS" \
    "$DATASET_ROOT/train" "$DATASET_ROOT/val"
}

resume_full() {
  local output="$EXPERIMENT_ROOT/full"
  local last_model="$output/saved_models/last_model.keras"
  local metrics="$output/logs/metrics/train_metrics.csv"
  local offset_file="$output/resume_epoch_offset"
  if [[ ! -f "$last_model" ]]; then
    echo "Resume model not found: $last_model" >&2
    exit 1
  fi
  local offset=0
  if [[ -f "$offset_file" ]]; then
    offset=$(<"$offset_file")
  fi
  local current=0
  if [[ -f "$metrics" ]]; then
    current=$(( $(wc -l < "$metrics") - 1 ))
    (( current < 0 )) && current=0
  fi
  local completed=$(( offset + current ))
  local remaining=$(( TARGET_EPOCHS - completed ))
  if (( remaining <= 0 )); then
    echo "Training already reached $TARGET_EPOCHS epochs"
    return
  fi
  echo "Resuming from $completed completed epochs; requesting $remaining more"
  printf '%s\n' "$completed" > "$offset_file"
  run_chain "$output" "$remaining" "$DATASET_ROOT/train" "$DATASET_ROOT/val" \
    training.resume_training=true "model.model_path=$last_model"
}

show_status() {
  echo "work_root=$WORK_ROOT"
  echo "modelzoo_commit=$MODELZOO_COMMIT"
  [[ -x "$PYTHON_BIN" ]] && "$PYTHON_BIN" --version || true
  [[ -f "$DATASET_ROOT/train_summary.json" ]] && cat "$DATASET_ROOT/train_summary.json" || true
  [[ -f "$DATASET_ROOT/val_summary.json" ]] && cat "$DATASET_ROOT/val_summary.json" || true
  find "$EXPERIMENT_ROOT" -maxdepth 3 -type f \
    \( -name '*.keras' -o -name '*.tflite' -o -name 'train_metrics.csv' \) -print 2>/dev/null || true
}

command=${1:-}
case "$command" in
  setup) setup_environment ;;
  prepare) prepare_dataset ;;
  smoke) run_smoke ;;
  train) run_full ;;
  resume) resume_full ;;
  status) show_status ;;
  all)
    setup_environment
    prepare_dataset
    run_smoke
    run_full
    ;;
  *) usage ;;
esac
