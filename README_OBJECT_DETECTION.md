# SAFRAME — STM32 Model Zoo物体検出

> この文書は物体検出（ST-YOLOX）版の記録です。現在のYuNet顔検出版は
> [README_FACE_DETECTION.md](README_FACE_DETECTION.md)を参照してください。

STM32N6570-DK上のμT-Kernel 3.0 BSP2で、カメラ画像をNeural-ARTへ入力し、
検出したperson領域をLCD上でマスクまたはモザイク化します。

## 現在の固定構成

- モデル: ST-YOLOX nano、480 x 480、UINT8入力／INT8出力、person 1クラス
- 生成元: STM32 AI Model Zoo Services 4.1系と
  `STM32N6-GettingStarted-ObjectDetection` v2.3.0
- 後処理: confidence 0.6、NMS 0.5、最大10件
- 入出力: IMX335カメラ、STM32N6570-DK LCD
- 実行: Vision、Render、Control/Monitorの3つのμT-Kernelタスク

既存のカメラ、LCD、Neural-ARTランタイム、dev-boot構成は維持し、BlazeFaceの
モデル生成物と後処理だけをModel Zooの物体検出構成へ置き換えています。
`Appli/FaceDetection`というディレクトリ名は既存CubeIDEプロジェクトとの互換性の
ため残しています。

## ビルドと実行

1. STM32CubeIDEへルート、Appli、FSBLの3プロジェクトをimportします。
2. AppliとFSBLをDebug構成でビルドします。
3. Development modeで、モデル重みを外部NORへ一度書き込みます。

```bash
export STM32N6_LOADER="<STM32CubeProgrammer>/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr"
STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -el "$STM32N6_LOADER" -hardRst \
  -w Appli/FaceDetection/Model/network_data.hex
```

4. `Appli/mtk3bsp2_stm32n657_Appli Debug.launch`を開始し、`usermain`で停止後に
   Resumeします。T-Monitorコンソールは115200 bps、8-N-1です。
5. B2を押すとMASKとMOSAICが切り替わります。

## Model Zooでモデルを差し替える

Model Zoo Servicesの公式STM32N6テンプレート上で生成し、その生成物をこの
μT-Kernelプロジェクトへimportします。

```bash
git clone --depth 1 https://github.com/STMicroelectronics/stm32ai-modelzoo-services.git
cd stm32ai-modelzoo-services
git submodule update --init application_code/object_detection/STM32N6

export MODELZOO_SERVICES_ROOT="$PWD"
export MODEL_PATH="<量子化済みモデル.tflite または .onnx>"
export STEDGEAI_PATH="<STEdgeAI>/Utilities/<platform>/stedgeai"
export CUBEIDE_PATH="<STM32CubeIDE実行ファイル>"
cp <このリポジトリ>/modelzoo/deployment_n6_object_detection.yaml \
  object_detection/modelzoo_saframe.yaml
cd object_detection
python stm32ai_main.py --config-path . \
  --config-name modelzoo_saframe.yaml
```

生成後、このリポジトリで次を実行します。

```bash
tools/import_modelzoo_object_detection.sh \
  "$MODELZOO_SERVICES_ROOT/application_code/object_detection/STM32N6"
```

YOLOv8を使う場合はYAMLの`model_type`を`yolov8n`へ変更します。YOLOv8、
YOLOv11、YOLO26は同じ`POSTPROCESS_OD_YOLO_V8_UI`経路を使うため、対応する
後処理ソースはこのリポジトリへ収録済みです。モデル変更後は必ずAppliを再ビルドし、
新しい`network_data.hex`を再書込みしてください。

## 確認済み範囲と残作業

ホスト上のプライバシーフィルタ境界テストは次で実行できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/privacy_filter_test.c \
  Appli/FaceDetection/Src/privacy_filter.c -o /tmp/privacy_filter_test
ASAN_OPTIONS=detect_leaks=0 /tmp/privacy_filter_test
```

実機での連続動作、フレームレート、480 x 480モデルの推論時間は未確認です。
計画書の320 x 320・30 ms目標は、まずこの固定モデルで一連の経路を確認した後、
Model Zooのbenchmarking結果を基にモデルを選定して検証します。
