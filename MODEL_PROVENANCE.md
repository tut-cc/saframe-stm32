# モデルの生成元調査メモ (Model Provenance Notes)

STM32N6 GettingStarted系デモ（`STM32N6-GettingStarted-FaceDetection` /
`STM32N6-GettingStarted-ObjectDetection`）に同梱されている `.tflite` /
`.onnx` モデルが、どのような設定（ST Model Zoo `stm32ai-modelzoo-services`
の `user_config.yaml`）で生成されたのかを調査したメモです。このファイルは
複数のリポジトリ・複数のセッションで共有される想定のため、「このリポジトリ」
という曖昧な表現は使わず、対象を毎回リポジトリ名で明示しています。

`STM32N6-GettingStarted-FaceDetection` / `STM32N6-GettingStarted-ObjectDetection`
のどちらも、`.tflite` ファイル自体に生成元のconfigを埋め込んでいません
（`strings` 抽出で確認済み。TFLite標準の `CONVERSION_METADATA` と
`min_runtime_version` しか含まれていない）。以下はすべて公開されている
`STMicroelectronics/stm32ai-modelzoo` と `STMicroelectronics/stm32ai-modelzoo-services`
のGitHubリポジトリを突き合わせて得た情報です。

---

## 1. `blazeface_front_128_quant_pc_ff_od_wider_face.tflite`

**所在:** `STM32N6-GettingStarted-FaceDetection` リポジトリの `Model/` フォルダ。

**状態: 推定の再構成。原本ではない。** `STMicroelectronics/stm32ai-modelzoo`
が公開しているのは「使うための」config（`operation_mode: prediction`）のみで、
学習・量子化した際のconfigは非公開。

- 発見場所: `STMicroelectronics/stm32ai-modelzoo` リポジトリの
  `object_detection/facedetect_front/Public_pretrainedmodel_public_dataset/widerface/blazeface_front_128_quant_pc_ff_od_widerface.tflite`
  （ファイル名/中身はほぼ同一だが、バイト単位での完全一致は未確認）。
- 元となった float モデル: BlazeFace（Google MediaPipeのアーキテクチャ）を
  [PINTO0309/PINTO_model_zoo](https://github.com/PINTO0309/PINTO_model_zoo/tree/main/030_BlazeFace)
  経由で取得し、WIDER FACE (frontal) で学習したもの。
- ファイル名タグの意味（`STMicroelectronics/stm32ai-modelzoo-services` リポジトリの
  `face_detection/tf/src/quantization/quantize.py` と
  `face_detection/docs/README_QUANTIZATION.md` で裏取り済み）:

  | タグ | 意味 | 対応する quantization: 設定値 |
  |---|---|---|
  | `blazeface_front_128` | model_type + 入力128×128 | `model_type: face_detect_front` |
  | `quant` | Post-Training Quantization済み | `quantization_type: PTQ` |
  | `pc` | チャネル単位の量子化粒度 | `granularity: per_channel` |
  | `ff` | 入出力はfloatのまま（重み/活性化のみint8） | `quantization_input_type: float`, `quantization_output_type: float` |
  | `od_wider_face` | 較正データセット＝物体検出(od)形式のWIDER FACE | `dataset.quantization_path: .../od_wider_face/...` |

- 使用された量子化器: `TFlite_converter`（`tf.lite.TFLiteConverter.from_keras_model`）。
  YuNetが使う `onnx_quantizer` ではない。
- 再構成した `quantization:` 相当のconfig（`STMicroelectronics/stm32ai-modelzoo-services`
  の `face_detection` ユースケースで使う想定）:

  ```yaml
  operation_mode: quantization
  model:
    model_type: face_detect_front
    model_path: <float Kerasモデル, 入力128x128>
  dataset:
    class_names: [face]
    quantization_path: .../od_wider_face/...
  preprocessing:
    rescaling: { scale: 1/255, offset: 0 }
    resizing: { aspect_ratio: fit, interpolation: bilinear }
    color_mode: rgb
  quantization:
    quantizer: TFlite_converter
    granularity: per_channel
    quantization_type: PTQ
    quantization_input_type: float
    quantization_output_type: float
    export_dir: quantized_models
  postprocessing:
    confidence_thresh: 0.5
    NMS_thresh: 0.5
    IoU_eval_thresh: 0.5
    max_detection_boxes: 10
  ```

- 留意点: Hugging Face上のSTモデルカード（`STMicroelectronics/face_detect_front`）
  には入力レンジが -1〜1 と書かれているが、`STMicroelectronics/stm32ai-modelzoo`
  公式configの `rescaling: {scale: 1/255, offset: 0}`（0〜1レンジ）と矛盾する。
  HF側の記述がテンプレ的で不正確な可能性が高い。
- アプリ側の実際の値（`STM32N6-GettingStarted-FaceDetection` リポジトリの
  `Application/*/Inc/app_config.h`）:
  `COLOR_MODE = RGB`、`AI_FD_BLAZEFACE_PP_CONF_THRESHOLD = 0.8`、
  `AI_FD_BLAZEFACE_PP_IOU_THRESHOLD = 0.5`、`ASPECT_RATIO_MODE = CROP`
  （学習時の `resizing.aspect_ratio: fit` とは異なる — 実機側のクロップ設定は
  デプロイ時に独立して決められるもの）。
  デプロイコマンド（`STM32N6-GettingStarted-FaceDetection` リポジトリの
  `Model/generate-n6-model_STM32N6570-DK.sh`）:
  `stedgeai generate --model blazeface_front_128_quant_pc_ff_od_wider_face.tflite --target stm32n6 --st-neural-art default@user_neuralart_STM32N6570-DK.json --input-data-type uint8 --output-data-type int8`

---

## 2. YuNet置き換え用config — `user_config_yunet.yaml`

**所在:** `STM32N6-GettingStarted-FaceDetection` リポジトリの
`Model/user_config_yunet.yaml`（作成済み）。

`STM32N6-GettingStarted-FaceDetection` リポジトリのBlazeFaceモデルを
`STMicroelectronics/stm32ai-modelzoo` のYuNetプリトレインモデルに置き換える
ために作成。preprocessing/postprocessingは上記1.で再構成したBlazeFace用config
に合わせてある。

- 使用したプリトレインモデル: `STMicroelectronics/stm32ai-modelzoo` リポジトリの
  `face_detection/yunet/Public_pretrainedmodel_public_dataset/widerface/yunetn_320/yunetn_320_qdq_int8.onnx`
  （入力320×320、既にper-channel／float I/Oで量子化済みのONNX。`.onnx`も
  `.tflite`と同様に `stedgeai generate` にそのまま使える）。
- **このセッション中で訂正した点:** 当初、同ディレクトリの
  `yunetn_320_eqe_config.yaml`（`STMicroelectronics/stm32ai-modelzoo` リポジトリ内）
  を「このモデルの生成config」として説明したが、よく見るとこのyaml自身の
  `model.model_path` が**既に量子化済みの** `yunetn_320_qdq_int8.onnx` を
  指しており（`operation_mode: chain_eqe` だが `quantize.py` 内の
  `model_is_quantized()` により量子化ステップは実質スキップされる）、これは
  使用/評価用configであって生成元のconfigではない。`quantization:` セクション
  （`granularity: per_channel`, `quantization_input_type/output_type: float`）は
  「おそらくこの設定で生成されただろう」という状況証拠にすぎない —
  BlazeFaceの場合と同じ限界がある。
- `user_config_yunet.yaml` の `preprocessing`/`postprocessing` は、1.で
  再構成したBlazeFaceの値（`resizing: fit + bilinear`、`color_mode: rgb`、
  `confidence_thresh/NMS_thresh/IoU_eval_thresh: 0.5`、
  `max_detection_boxes: 10`）に合わせた。結果的にST公式の
  `deployment_n6_yunet_config.yaml`（`STMicroelectronics/stm32ai-modelzoo-services`
  リポジトリ内）のデフォルト値と全く同じだったため、実質的な上書きは
  発生しなかった。
- `deployment.c_project_path` はモデルズーの汎用テンプレートパスではなく、
  `STM32N6-GettingStarted-FaceDetection` リポジトリの
  `Application/STM32N6570-DK/` を指すように変更した。
- **未対応（フラグのみ、実装はしていない）:** YuNetは入力320×320
  （BlazeFaceは128×128）のため、`STM32N6-GettingStarted-FaceDetection`
  リポジトリの `Model/generate-n6-model_*.sh`、`Model/user_neuralart_*.json`、
  `app_config.h` 内の `AI_FD_BLAZEFACE_PP_*` 定義群（画像サイズ・アンカー・
  `POSTPROCESS_TYPE`）はすべて修正が必要。後処理ライブラリ自体は既にYuNet対応済み
  （同リポジトリの `Middlewares/ai-postprocessing-wrapper/app_postprocess_fd_yunet_ui.c`、
  `Middlewares/stm32-vision-models-postprocessing/.../fd_pp_yunet.c`）なので、
  C側の設定・配線のみが未対応。
- **`user_config_yunet.yaml` というファイルは削除済み。** 以下がその全文
  （このメモ内に転記済みなので、ファイル自体は存在しなくても内容は再現できる）:

  ```yaml
  # user_config.yaml for stm32ai-modelzoo-services (face_detection use case)
  #
  # Purpose: replace blazeface_front_128_quant_pc_ff_od_wider_face.tflite in the
  # STM32N6-GettingStarted-FaceDetection repo with the ST Model Zoo "yunet" pretrained model
  # (yunetn_320_qdq_int8.onnx), keeping preprocessing/postprocessing aligned with
  # the reconstructed config of the original blazeface model (rescaling/resizing
  # = fit+bilinear+rgb, confidence_thresh/NMS_thresh/IoU_eval_thresh = 0.5,
  # max_detection_boxes = 10 -- these already match ST's own yunet deployment
  # defaults, so no override was needed for those fields).
  #
  # Run with (from the stm32ai-modelzoo-services/face_detection/ directory):
  #   python stm32ai_main.py --config-path <path-to-this-file's-dir> --config-name user_config_yunet.yaml

  general:
    display_figures: True

  operation_mode: deployment   # onnx models can be fed to `stedgeai generate` exactly like .tflite

  model:
    model_type: yunet
    # Path to the pretrained, already per-channel/float-IO quantized ONNX model
    # (see stm32ai-modelzoo/face_detection/yunet/Public_pretrainedmodel_public_dataset/widerface/yunetn_320/yunetn_320_qdq_int8.onnx).
    # Adjust this path to wherever you copied the .onnx file locally (e.g. the
    # STM32N6-GettingStarted-FaceDetection repo's Model/ folder).
    model_path: ../../stm32ai-modelzoo/face_detection/yunet/Public_pretrainedmodel_public_dataset/widerface/yunetn_320/yunetn_320_qdq_int8.onnx

  dataset:
    class_names: [face]

  preprocessing:
    resizing:
      aspect_ratio: fit
      interpolation: bilinear
    color_mode: rgb

  postprocessing:
    confidence_thresh: 0.5
    NMS_thresh: 0.5
    IoU_eval_thresh: 0.5
    max_detection_boxes: 10
    crop_stretch_percents: [0,40,0,40] # stretch_xmin%, stretch_ymin%, stretch_xmax%, stretch_ymax%

  tools:
    stedgeai:
      optimization: balanced
      on_cloud: False
      path_to_stedgeai: C:/ST/STEdgeAI/4.0/Utilities/windows/stedgeai.exe
    path_to_cubeIDE: C:/ST/STM32CubeIDE_<*.*.*>/STM32CubeIDE/stm32cubeide.exe

  deployment:
    # Point this at the Application/<board> folder of the
    # STM32N6-GettingStarted-FaceDetection repo, not the generic model-zoo
    # "application_code" template, so the generated network files land in the
    # project that will actually be built.
    c_project_path: ../Application/STM32N6570-DK/
    IDE: GCC
    verbosity: 1
    hardware_setup:
      serie: STM32N6
      board: STM32N6570-DK

  mlflow:
    uri: experiments_outputs/mlruns

  hydra:
    run:
      dir: experiments_outputs/${now:%Y_%m_%d_%H_%M_%S}
  ```

---

## 3. `st_yolo_x_nano_480_1.0_0.25_3_st_int8.tflite`

**所在:** `STM32N6-GettingStarted-ObjectDetection` リポジトリの `Model/` フォルダ
（`STM32N6-GettingStarted-FaceDetection` とは別のリポジトリ）。

**状態: かなり近い「本物の学習config」を発見。** 上記1.・2.と違い、こちらは
usage configではなく学習・量子化・評価・ベンチマークを一括実行するconfigが
見つかった。

- 発見場所: `STMicroelectronics/stm32ai-modelzoo` リポジトリの
  `object_detection/st_yoloxn/ST_pretrainedmodel_custom_dataset/st_person/st_yoloxn_d100_w025_480/st_yoloxn_d100_w025_480_config.yaml`
  （同ディレクトリに `st_yoloxn_d100_w025_480_int8.tflite`、float版の`.keras`、
  `_qdq_int8.onnx`、混合精度版 `_qdq_w4...a8...onnx` も同居）。
- 強い一致の根拠: このconfigの `general.project_name:
  st_yoloxn_480_1.0_0.25_3_st_official` が、`STM32N6-GettingStarted-ObjectDetection`
  リポジトリ側のファイル名 `st_yolo_x_nano_480_1.0_0.25_3_st_int8.tflite` と
  ほぼ一致（`st_yoloxn` → `st_yolo_x_nano` にリネーム、`_official` を省略）。
  ファイル名がバイト単位で同一とまでは確認できていないが、今回調査した
  3モデルの中で最も一致度が高い。
- `operation_mode: chain_tqeb`（学習→量子化→評価→ベンチマークを一括実行）—
  同フォルダの `.keras` float モデルはこのconfigの学習結果と考えられる。
- ファイル名タグの意味（このconfig自身のフィールドと、
  `STM32N6-GettingStarted-ObjectDetection` リポジトリの
  `Application/STM32N6570-DK/Inc/app_config.h` で裏取り済み）:

  | タグ | 意味 | 根拠 |
  |---|---|---|
  | `480` | 入力解像度 480×480 | `model.input_shape: (480,480,3)` |
  | `1.0` | depth multiplier | `model_name: st_yoloxn_d100_w025`（d100=1.00） |
  | `0.25` | width multiplier | 同上（w025=0.25） |
  | `3` | **クラス数ではなくアンカー数**（クラスは`person`1つのみ） | `postprocessing.yolo_anchors` が(w,h)ペア3組、app側の `AI_OD_ST_YOLOX_PP_NB_ANCHORS (3)` とも一致 |
  | `st` | ST独自データセット(`st_person`)で学習（公開COCO-personではない） | `dataset.dataset_name: custom_dataset` |
  | `int8` | TFLiteConverterによるPTQ量子化 | `quantization.quantizer: TFlite_converter` |

- 全文（`st_yoloxn_d100_w025_480_config.yaml`、`STMicroelectronics/stm32ai-modelzoo`
  リポジトリ内）。今回の調査で最も「原本」に近いと考えられるので、そのまま転記:

  ```yaml
  general:
    project_name: st_yoloxn_480_1.0_0.25_3_st_official
    display_figures: false
    gpu_memory_limit: 24
    num_threads_tflite: 24
    global_seed: 127

  model:
    model_type: st_yoloxn
    model_name: st_yoloxn_d100_w025
    input_shape: (480,480,3)
    pretrained: false

  operation_mode: chain_tqeb

  dataset:
    dataset_name: custom_dataset
    class_names: [person]
    format: tfs
    train_images_path: /local/data/od_st_person/train
    train_annotations_path: /local/data/od_st_person/train
    val_images_path: /local/data/od_st_person/test
    val_annotations_path: /local/data/od_st_person/test
    test_images_path: /local/data/od_st_person/test
    test_annotations_path: /local/data/od_st_person/test
    quantization_path: /local/data/od_st_person/train
    quantization_split: 0.001

  preprocessing:
    rescaling: { scale: 1/255, offset: 0 }
    resizing: { aspect_ratio: fit, interpolation: nearest }
    color_mode: rgb

  data_augmentation:
    random_crop:
      crop_center_x: (0.25, 0.75)
      crop_center_y: (0.25, 0.75)
      crop_width: (0.6, 0.9)
      crop_height: (0.6, 0.9)
      change_rate: 0.5
    random_periodic_resizing:
      period: 20
      image_sizes: [(384,384),(416,416),(448,448),(480,480),(512,512),(544,544),(576,576),(608,608)]
    random_contrast: { factor: 0.4 }
    random_brightness: { factor: 0.3 }
    random_flip: { mode: horizontal }
    random_rotation: { factor: 0.02, fill_mode: wrap, interpolation: nearest }

  training:
    dropout: null
    batch_size: 64
    epochs: 750
    optimizer: { Adam: { learning_rate: 0.0025 } }
    callbacks:
      LRWarmupCosineDecay:
        initial_lr: 1.0e-05
        warmup_steps: 20
        max_lr: 0.001875
        hold_steps: 30
        decay_steps: 650
        end_lr: 1.0e-06
      EarlyStopping:
        monitor: val_map
        patience: 100
        restore_best_weights: true
        verbose: 1

  postprocessing:
    yolo_anchors: [0.5, 0.5, 0.07, 0.25, 0.23, 0.7]
    confidence_thresh: 0.001
    NMS_thresh: 0.5
    IoU_eval_thresh: 0.5
    plot_metrics: false
    max_detection_boxes: 100

  quantization:
    quantizer: TFlite_converter
    quantization_type: PTQ
    quantization_input_type: uint8
    quantization_output_type: float
    export_dir: quantized_models

  tools:
    stedgeai:
      optimization: balanced
      on_cloud: true
      path_to_stedgeai: C:/ST/STEdgeAI/<x.y>/Utilities/windows/stedgeai.exe
    path_to_cubeIDE: C:/ST/STM32CubeIDE_<*.*.*>/STM32CubeIDE/stm32cubeide.exe

  benchmarking:
    board: STM32N6570-DK

  mlflow:
    uri: ./tf/src/experiments_outputs/mlruns

  hydra:
    run:
      dir: ./tf/src/experiments_outputs/${now:%Y_%m_%d_%H_%M_%S}
  ```

- アプリ側の実際の値（`STM32N6-GettingStarted-ObjectDetection` リポジトリの
  `Application/STM32N6570-DK/Inc/app_config.h`）:
  `NB_CLASSES = 1`（`"person"`）、`ASPECT_RATIO_MODE = ASPECT_RATIO_CROP`
  （学習時の `resizing.aspect_ratio: fit` とはまた別 — BlazeFaceと同じ
  「学習時前処理 ≠ 実機デプロイ時クロップ設定」の構図）、
  `AI_OD_ST_YOLOX_PP_CONF_THRESHOLD = 0.6`、`AI_OD_ST_YOLOX_PP_IOU_THRESHOLD = 0.5`。
  `STMicroelectronics/stm32ai-modelzoo-services` リポジトリの
  `deployment_n6_st_yoloxn_config.yaml` サンプル（別バリアント `d033_w025_320`
  用）では `resizing: { aspect_ratio: crop, interpolation: nearest }` と
  なっており、アプリ側の `ASPECT_RATIO_CROP` と一致する — つまり**デプロイ用
  config**の `preprocessing.resizing.aspect_ratio` は学習時の `fit` ではなく
  `crop` にすべき、ということ。
  デプロイコマンド（`STM32N6-GettingStarted-ObjectDetection` リポジトリの
  `Model/generate-n6-model_STM32N6570-DK.sh`）:
  `stedgeai generate --model st_yolo_x_nano_480_1.0_0.25_3_st_int8.tflite --target stm32n6 --st-neural-art default@user_neuralart_STM32N6570-DK.json --input-data-type uint8 --output-data-type int8`

---

## 3モデル共通で見えたパターン

1. `STM32N6-GettingStarted-FaceDetection` / `STM32N6-GettingStarted-ObjectDetection`
   のどちらのアプリ側リポジトリも、モデルファイル自体にもリポジトリ内にも
   生成元configを含んでいない。`STMicroelectronics/stm32ai-modelzoo` 側に
   あっても大抵は「使うための」config（prediction/evaluation/deployment）
   だけで、学習・量子化した原本のconfigは**`st_yoloxn_d100_w025_480` のケース
   を除いて**見つからなかった。このケースだけfloat `.keras` と量子化済み
   成果物のそばに `chain_tqeb`（学習込み）のconfigがまるごと残っており、
   唯一「原本」に近いと言える。
2. ST社の物体検出系モデル（`facedetect_front`、`st_yoloxn`）のファイル名タグは、
   解像度・depth/width multiplier（該当する場合）・量子化粒度やIO型もしくは
   アンカー数・較正/学習データセットを表す傾向があるが、**タグの意味はモデル
   ファミリーごとに異なる**ため、毎回そのモデル固有の `quantize.py` /
   READMEと突き合わせて確認する必要がある。
3. 学習時の `preprocessing.resizing.aspect_ratio`（大抵 `fit`）と、実機の
   `app_config.h` の `ASPECT_RATIO_MODE`（大抵 `crop`）は独立した別設定であり、
   一致するとは限らない。
