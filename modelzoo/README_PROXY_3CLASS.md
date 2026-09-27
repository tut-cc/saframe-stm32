# COCO proxy 3-class ST-YOLOX 320

This configuration deliberately uses three COCO objects as stand-ins for the
SAFRAME demonstration:

| Model class | COCO source category | Display alias |
|---:|---|---|
| 0 | `person` (category 1) | `FACE` |
| 1 | `book` (category 84) | `DOCUMENT` |
| 2 | `stop sign` (category 13) | `LOGO` |

The aliases do not change model semantics. The model detects whole persons,
books, and stop signs; it does not become a face, general-document, or arbitrary
logo detector.

## Prepare COCO annotations

Download COCO 2017 `train2017`, `val2017`, and `annotations_trainval2017`.
Create filtered annotations while continuing to use the original image
directories:

```bash
python3 tools/extract_coco_proxy_classes.py \
  <coco>/annotations/instances_train2017.json \
  <coco>/annotations/instances_train2017_proxy3.json \
  --images <coco>/train2017 --darknet-output <work>/proxy3/train
python3 tools/extract_coco_proxy_classes.py \
  <coco>/annotations/instances_val2017.json \
  <coco>/annotations/instances_val2017_proxy3.json \
  --images <coco>/val2017 --darknet-output <work>/proxy3/val
```

The generated JSON files contain only images with at least one selected object
and remap category IDs to contiguous IDs 1, 2, 3. The Darknet YOLO directories
contain relative image symlinks and zero-based text labels in the required
order. Model Zoo Services converts these labels to its serialized `.tfs`
format when the chain starts.

## Train and quantize

For Google Colab, upload and run
`notebooks/SAFRAME_COCO_Proxy3_Colab.ipynb`. Select a GPU runtime and edit only
`DRIVE_ROOT`. The notebook pins Model Zoo Services, checks for at least 45 GiB
of ephemeral disk, downloads COCO outside Drive, runs a one-epoch smoke chain,
and provides separate full-training and resume cells. Checkpoints, reports, and
models are persisted below `DRIVE_ROOT/experiments`; credentials are never
written by the notebook. It creates an isolated Python 3.11 environment because
the pinned TensorFlow 2.18 package does not provide a Python 3.13 wheel.
The isolated processes use Matplotlib's non-interactive `Agg` backend so they
do not inherit Colab's kernel-only `matplotlib_inline` backend.

Use the TensorFlow object-detection pipeline from STM32 AI Model Zoo Services.
`training_coco_proxy_3class.yaml` is derived from the official
`st_yoloxn_d033_w025_320_config.yaml`; its baseline training and augmentation
values are unchanged. Run it from the Model Zoo Services object-detection
directory:

```bash
export PROXY3_TRAIN_DIR=<work>/proxy3/train
export PROXY3_VAL_DIR=<work>/proxy3/val
export STEDGEAI_PATH=<STEdgeAI>/Utilities/<platform>/stedgeai
export CUBEIDE_PATH=<STM32CubeIDE executable>
cp <this-repo>/modelzoo/training_coco_proxy_3class.yaml .
python stm32ai_main.py --config-path . \
  --config-name training_coco_proxy_3class.yaml
```

Use `operation_mode=chain_tqe` on the command line when N6 board/cloud
benchmarking is unavailable.

Model Zoo Services produces the float/quantized evaluation reports, including
per-class metrics and the confusion matrix, below its experiment output
directory. Do not tune thresholds or training hyperparameters until this
baseline has been recorded.

## Generate and import the STM32N6 application

The CubeIDE download cache on the development machine already contains the
required STEdgeAI packages. Install them outside the repository with:

```bash
tools/setup_stedgeai_core.sh \
  /home/kobas/.cache/qt-installer-framework \
  /home/kobas/.cache/saframe-stedgeai/4.0.1
export STEDGEAI_PATH=/home/kobas/.cache/saframe-stedgeai/4.0.1/Utilities/linux/stedgeai
```

Point `MODEL_PATH` at the resulting quantized TFLite model and deploy with
`modelzoo/deployment_n6_object_detection.yaml`, as documented in
`README_OBJECT_DETECTION.md`. Importing is intentionally strict:

```bash
tools/import_modelzoo_object_detection.sh \
  "$MODELZOO_SERVICES_ROOT/application_code/object_detection/STM32N6"
```

The importer rejects anything other than a 320 x 320 RGB, three-class model
whose class table is exactly `person, book, stop sign`. It also regenerates
the runtime XSPI weight signature from the new `network_data.hex`.

After importing, rebuild Appli, write the new `network_data.hex` to XSPI2,
and run the hardware acceptance procedure in `README_OBJECT_DETECTION.md`.
