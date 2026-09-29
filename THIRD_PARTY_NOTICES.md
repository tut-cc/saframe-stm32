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
- STM32 AI Model Zoo: https://github.com/STMicroelectronics/stm32ai-modelzoo
- 320 x 320 COCO-Personモデル固定コミット: `1423c78953a830903485135febe1dd98ff31aed8`
- `st_yoloxn_d033_w025_320_int8.tflite` SHA-256:
  `e5a8a27200ba0d6ad7e0099c7bb373e605df7320647cd265d186788f1823fbfa`
- Neural-ART生成およびNPUランタイム: STEdgeAI Core 4.0.1／STM32 MCU 12.0.1
  (`ll_aton` 1.1.3-dev275)
- 使用範囲: ST-YOLOXモデルと重み、Neural-ART生成コード、物体検出後処理
- ライセンス: 各ソースファイルおよび上記リポジトリのライセンスを参照

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

## STM32N6 UVC Library / STM32 USB Device Library

- 提供者: STMicroelectronics
- UVC Library取得元: https://github.com/STMicroelectronics/STM32N6-GettingStarted-ObjectDetection
- UVC Library固定コミット: `4240acbeb7febd335cf128ebe56f26790cabbdd3`
  (`uvcl` v3.0.1)
- STM32 USB Device Library: https://github.com/STMicroelectronics/stm32-mw-usb-device
- STM32 USB Device Library固定コミット: `2a0a3521ac4d84e6e494d37bed615e2d36c373f5`
  (`v2.11.4`、Coreのみ)
- 使用範囲: USB UVCディスクリプタ、UVCフレーム転送、USB Device Core、
  STM32N6 PCD/LL USBドライバ
- ライセンス: `Appli/FaceDetection/Vendor/UVC/uvcl/LICENSE`、
  `Appli/FaceDetection/Vendor/UVC/STM32_USB_Device_Library/LICENSE.md`、
  および各ソースファイルのライセンス表示を参照

## Video Encoder (VENC) / EWL

- 提供者: Verisilicon Inc.、Google Inc.（Video Encoder）、STMicroelectronics（EWL、LL VENC）
- 取得元: STM32CubeN6 V1.3.0 の `Middlewares/Third_Party/VideoEncoder`、
  `Middlewares/ST/VideoEncoder_EWL`（V1.2.1）、
  `Drivers/STM32N6xx_HAL_Driver` の `stm32n6xx_ll_venc`
- 使用範囲: JPEG 符号化（`inc/`、`source/common/`、`source/jpeg/`）と EWL。H.264 は含めない
- ライセンス: Video Encoder は GPL-2.0 と BSD-3-Clause のデュアルライセンスで、
  本プロジェクトは BSD-3-Clause の条件で使用する。
  `Appli/FaceDetection/Vendor/VideoEncoder/LICENSE.txt`、
  `Appli/FaceDetection/Vendor/VideoEncoder_EWL/LICENSE.txt`、
  および各ファイルのライセンス表示を参照
