# Third-party notices

このプロジェクトは、次の既存コード資源を統合・改変して使用しています。
各ファイルに付された著作権表示とライセンス条件も併せて参照してください。

## STM32N6 Getting Started — Face Detection

- 提供者: STMicroelectronics
- リポジトリ: https://github.com/STMicroelectronics/STM32N6-GettingStarted-FaceDetection
- 固定バージョン: `v1.1.0`
- 固定コミット: `45faf18539c037a4cb766d2824041d9eba201519`
- 使用範囲: BlazeFaceモデルと重み、Neural-ART生成コード、カメラ／ISP／LCD
  パイプライン、後処理、STM32N6570-DK BSP
- ライセンス: 各ソースファイルおよび上記リポジトリのライセンスを参照

## STM32 AI Model Zoo Services / STM32N6 Getting Started — Object Detection

- 提供者: STMicroelectronics
- Model Zoo Services: https://github.com/STMicroelectronics/stm32ai-modelzoo-services
- Model Zoo Services固定コミット: `0f6210ed5156126b782e1c43249063a477484b20`
- Object Detectionテンプレート: https://github.com/STMicroelectronics/STM32N6-GettingStarted-ObjectDetection
- Object Detection固定コミット: `7ae96b5452183664c0d9b3dfe06a82a6ed0e59cb` (`v2.3.0`)
- 使用範囲: ST-YOLOXモデルと重み、Neural-ART生成コード、物体検出後処理
- ライセンス: 各ソースファイルおよび上記リポジトリのライセンスを参照

## STM32 AI Model Zoo — YuNet face detection

- 提供者: STMicroelectronics
- モデル: https://github.com/STMicroelectronics/stm32ai-modelzoo
  （`face_detection/yunet/Public_pretrainedmodel_public_dataset/widerface/yunetn_320/yunetn_320_qdq_int8.onnx`）
- 取得時コミット: `1423c78953a830903485135febe1dd98ff31aed8`
- 後処理実装（`fd_pp_yunet.c` / `app_postprocess_fd_yunet_ui.c`）と
  Neural-ART生成設定（`user_neuralart_STM32N6570-DK.json` /
  `my_mpools/stm32n6-app2_STM32N6570-DK.mpool`）は、上記「STM32N6 Getting
  Started — Face Detection」と同じ固定コミット（`45faf18539c037a4cb766d2824041d9eba201519`）
  から取得（内容が完全一致することを確認済み）
- 使用範囲: YuNetモデルと重み、Neural-ART生成コード、顔検出後処理
- ライセンス: 各ソースファイルおよび上記リポジトリのライセンスを参照
- `Vendor/Postprocess/Inc/fd_yunet_anchors_{32,16,8}.h`（グリッドアンカー座標
  テーブル）は、通常はModel Zoo Servicesのデプロイパイプラインが生成する
  ファイルだが、このリポジトリでは
  `STMicroelectronics/stm32ai-modelzoo-services`
  （`face_detection/tf/src/postprocessing/postprocess.py`の
  `generate_yunet_anchor()`、上記と同じ固定コミット）のアルゴリズムを
  Pythonデプロイパイプライン自体は動かさずに
  [tools/generate_yunet_anchors.py](tools/generate_yunet_anchors.py) として
  再実装し、その出力として生成したものである

## μT-Kernel 3.0 BSP2 / μT-Kernel 3.0

- 提供者: TRON Forum
- BSP2リポジトリ: https://github.com/tron-forum/mtk3_bsp2
- μT-Kernelリポジトリ: https://github.com/tron-forum/mtkernel_3
- 統合時のBSP2バージョン: `v1.00.04`
- 統合時のμT-Kernelバージョン: `v3.00.08`
- 使用範囲: カーネル、T-Monitor、STM32 Cube向けBSP2移植層
- ライセンス: T-License 2.2（一部BSP設定ファイルは各ヘッダに記載された
  T-License 2.1）
- ライセンス案内: https://www.tron.org/download/index.php?route=information/information&information_id=79

## STM32CubeN6 HAL / CMSIS

- 提供者: STMicroelectronics / Arm
- 使用範囲: `Drivers/`、`Middlewares/`、`Secure_nsclib/` 内の配布物
- ライセンス: `Drivers/STM32N6xx_HAL_Driver/LICENSE.txt`、
  `Drivers/CMSIS/LICENSE`、`Drivers/CMSIS/Device/ST/STM32N6xx/LICENSE.txt`、
  および各ファイルのライセンス表示を参照
