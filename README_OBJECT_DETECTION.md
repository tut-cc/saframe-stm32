# Saframe — STM32 Model Zoo物体検出

STM32N6570-DK上のμT-Kernel 3.0 BSP2で、カメラ画像をNeural-ARTへ入力し、
検出したperson領域をLCD上でマスクまたはモザイク化します。

## 現在の固定構成

- モデル: ST-YOLOX nano `d033_w025`、320 x 320、UINT8入力／INT8出力、person 1クラス
- 生成元: STM32 AI Model Zoo Services
  `0f6210ed5156126b782e1c43249063a477484b20`、STEdgeAI Core 4.0.1／
  STM32 MCU 12.0.1、`STM32N6-GettingStarted-ObjectDetection` v2.3.0
- NPUランタイム: STAI tools 4.0.1、LL_ATON 1.1.3-dev275、
  NetworkRuntime 12.0.1（生成物と同じSTEdgeAI配布物一式）
- 後処理: confidence 0.6、NMS 0.5、最大10件
- 入出力: IMX335カメラ、STM32N6570-DK LCD、USB1/CN18のH.264 UVC
- 実行: 次の6つのμT-Kernelタスク。優先度は数値が小さいほど高い

| タスク | 優先度 | 役割 |
| --- | ---: | --- |
| Render | 3 | MASK/MOSAIC、33 ms期限判定、LCD公開。USB用縮小コピーの間だけ6へ下げる |
| Capture | 4 | DCMIPP Pipe 1とPipe 2のフレームを組にして後段へ渡す |
| Inference | 5 | NN入力コピー、NPU推論、後処理。`usermain`のタスク |
| USB encoder | 7 | VENCでH.264へエンコードする |
| USB service | 8 | USB割込み後のHAL/UVC処理 |
| Control/Monitor | 12 | B2の切り替えと、1秒ごとのT-Monitor出力 |

既存のカメラ、LCD、Neural-ARTランタイム、dev-boot構成は維持し、BlazeFaceの
モデル生成物と後処理だけをModel Zooの物体検出構成へ置き換えています。
`Appli/FaceDetection`というディレクトリ名は既存CubeIDEプロジェクトとの互換性の
ため残しています。

取り込んだ既存コード、著作権表示、固定バージョン、ライセンス条件は
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)に記録しています。VENC/H.264の
取り込みパス、ローカル改変、参照のみのプログラムは
[VENC source provenance](Appli/FaceDetection/Vendor/VENC/SOURCE.md)に分けて記録しています。

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
git clone https://github.com/STMicroelectronics/stm32ai-modelzoo-services.git
cd stm32ai-modelzoo-services
git checkout --detach 0f6210ed5156126b782e1c43249063a477484b20
git submodule update --init application_code/object_detection/STM32N6

export MODELZOO_SERVICES_ROOT="$PWD"
export SAFRAME_ROOT="<このリポジトリの絶対パス>"
export MODEL_PATH="<st_yoloxn_d033_w025_320_int8.tflite>"
export STEDGEAI_PATH="<STEdgeAI>/Utilities/<platform>/stedgeai"
export CUBEIDE_PATH="<STM32CubeIDE実行ファイル>"
cd object_detection
MPLBACKEND=Agg python stm32ai_main.py \
  --config-path "$SAFRAME_ROOT/modelzoo" \
  --config-name deployment_n6_person_320.yaml \
  model.model_path="$MODEL_PATH" \
  tools.stedgeai.path_to_stedgeai="$STEDGEAI_PATH" \
  tools.path_to_cubeIDE="$CUBEIDE_PATH" \
  deployment.c_project_path=../application_code/object_detection/STM32N6/
```

Model Zoo Servicesの`requirements.txt`は専用Python環境にインストールします。
この固定リビジョンではYAMLの環境変数展開に依存せず、上のHydra overrideで
絶対パスを渡します。元TFLiteのリビジョンとSHA-256は
`modelzoo/st_yoloxn_person_320.json`に固定されています。

生成に使ったSTEdgeAIと組込み側ランタイムは一式で同期します。`stai.h`や
`ll_aton_version.h`だけの部分更新は禁止です。

```bash
tools/sync_stedgeai_runtime.sh "<STEdgeAI>/Middlewares/ST/AI"
```

生成後、このリポジトリで次を実行します。

```bash
tools/import_modelzoo_object_detection.sh \
  "$MODELZOO_SERVICES_ROOT/application_code/object_detection/STM32N6"
```

importスクリプトは指定した契約ファイルに従って入力形状、入出力型、クラス数と順序、
後処理、出力テンソルに加え、生成コードとLL_ATON、STAI、静的ランタイムの世代を
検査します。不一致な生成物は既存モデルを上書きする前に拒否し、新しい重みの実行時
署名も自動生成します。既定では現在のperson 1クラス契約
`modelzoo/st_yoloxn_person_320.json`を使用します。

YOLOv8を使う場合はYAMLの`model_type`を`yolov8n`へ変更します。YOLOv8、
YOLOv11、YOLO26は同じ`POSTPROCESS_OD_YOLO_V8_UI`経路を使うため、対応する
後処理ソースはこのリポジトリへ収録済みです。モデル変更後は必ずAppliを再ビルドし、
新しい`network_data.hex`を再書込みしてください。

## 確認済み範囲と残作業

### 安全化済みフレーム公開ゲート

この構成を選んだ理由、棄却した方式、保証する状態遷移は
[ADR 0006](docs/adr/0006-publish-only-sanitized-frames.md)に記録しています。

LCD背景はRGB565の5面バッファ、NN入力はPSRAM上の3面ステージングで管理します。
DCMIPPはPipe 1とPipe 2を連続モードで30 fps動かし、VSYNC割込みで次のフレームの
書き込み先を非公開の作業バッファへ切り替えます。空きバッファがないフレームは捨て用の
バッファへ書き込んで破棄します。Pipe 1とPipe 2の完了時刻の差が半フレーム以内の
ものだけを同じフレームとして組み、組めなかったフレームも破棄します。

推論とマスク／モザイク処理が完了したフレームだけを、VBlankで表示バッファへ
切り替えます。期限の起点は、2本目のpipeのフレーム完了割込みで読んだDWT cycle
counterの値です。そこから33 msを超えたフレームは破棄し、LCDは直前の安全化済み
フレームを保持します。最初の安全化済みフレームが完成するまでは黒画面です。

起動時はセンサーを安定して開始するため、Pipe 1で非公開バッファへ最初のsnapshotを
1枚取得して破棄し、その後にPipe 1とPipe 2の連続取り込みを開始します。タイム
アウト時のT-Monitor出力にはPipe別の完了回数を表示するため、どちらの経路で停止した
かを判別できます。IMX335への設定書き込みがI2C NACKで失敗した場合は3回まで
再試行し、それでも失敗したときは停止せずに次のフレームで設定し直します。

T-Monitorには1秒ごとに`OD:`行を出力します。時間はマイクロ秒単位で、処理中では
なく最後に完了したフレームの値です。

| 項目 | 意味 |
| --- | --- |
| `capture`、`capture_lag`、`capture_wait` | 取り込み時間、完了割込みからCaptureタスクが受け取るまで、キューでの待ち |
| `copy`、`ai`、`pp` | NN入力コピー、NPU推論、後処理 |
| `filter`、`render`、`total` | MASK/MOSAIC本体、Render全体、期限判定の対象となる合計 |
| `overlay`、`usb_submit`、`isp` | 状態表示の描画、USB用縮小コピー、ISP更新 |
| `captured_fps` から `published_fps` | 各段の1秒あたりの処理数 |
| `published`、`dropped`、`max_consecutive`、`max_published_total` | 公開数、期限超過で破棄した数、最大連続破棄数、公開したフレームの最大`total` |
| `capture_drops`、`unpaired` | 空きバッファ不足で捨てた数、Pipe 1/Pipe 2が組めずに捨てた数 |
| `isp_errors`、`sensor_retries`、`sensor_failures` | ISPとセンサー設定の失敗と再試行 |
| `uvc`、`uvc_encoded_fps`、`uvc_repeated` | USBの状態、H.264のエンコード数、直前の画像を再送した数 |

HardFault、MemManage、BusFault、UsageFaultまたはアプリケーションassertが発生した
場合は、障害種別、処理段階、フレーム番号、PC/LR/SP、CFSR/HFSR/MMFAR/BFARを
`.noinit`へ保存します。dev-bootで再起動すると、前回障害をT-Monitorへ`FAULT:`で
出力します。

記録されたPCは、使用中のGNU Arm Embedded Toolchainで次のようにソース位置へ変換
できます（`0x...`は`FAULT:`行のPCへ置き換えます）。

```bash
arm-none-eabi-addr2line -f -C \
  -e Appli/Debug/mtk3bsp2_stm32n657_Appli.elf 0x...
```

期限超過経路を実機確認する場合は、Appliのコンパイラ
定義へ次を一時的に追加してください。

```text
PRIVACY_TEST_RENDER_DELAY_MS=40
```

この定義では安全化処理へ40 msの試験遅延が入り、`dropped`が増えてLCD背景が
切り替わらないことを確認できます。通常ビルドでは未定義、すなわち0 msです。

安全化済みフレームはLCDに加えて、USB1 Type-CコネクタCN18からH.264専用UVC
Webカメラとして出力します。形式は320 x 240、30 fps、約1 Mbps VBR、GOP 30です。
Renderタスクは、LCDへ公開した800 x 480 RGB565表示フレームの中央640 x 480を
2画素おきにUSB専用二面入力へコピーします。このコピーはホストがストリーミング中の
ときだけ行い、その間はRenderタスクの優先度を推論より低い6へ下げます。VENCタスクは
新しい入力が届くたびにエンコードし、50 ms以内に届かなければ直近の安全画像を
再エンコードします。最初の安全化完了前は黒画像を送ります。RAWフレーム、
期限超過画像、モデル不正時の画像へ切り替わる経路はありません。

USBのHAL/UVC処理は割込み内ではなく、優先度8の専用タスクで行います。VENCタスクは
優先度7です。どちらもLCDのRender/Capture、推論より低い優先度のため、USBホストが
ストリーミングを開始してもLCD更新を優先します。

CN18とホストをUSB Type-Cケーブルで接続し、LinuxではVLC/guvcview、Windowsでは
FFmpeg `ffplay`を使用してください。T-Monitorでは`uvc_encoded_fps`、
`uvc_repeated`、`uvc_encode_dropped`、`uvc_last_bytes`、`uvc_encode_max_us`、
`uvc_idr_count`を確認できます。USB出力を保持するホストアプリでは30 fps付近を維持し、
推論・安全化の公開fpsが30未満なら`uvc_repeated`が増えます。Windows標準カメラなど
MJPEGのみ対応するアプリ、1280 x 720、USBX、USB DMAは今回の対象外です。

ホスト上のプライバシーフィルタ境界テストは次で実行できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/privacy_filter_test.c \
  Appli/FaceDetection/Src/privacy_filter.c -o /tmp/privacy_filter_test
ASAN_OPTIONS=detect_leaks=0 /tmp/privacy_filter_test
```

RGB565の中央640 x 480クロップと320 x 240への2画素間引きは、次でホスト試験できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/usb_webcam_conversion_test.c \
  Appli/FaceDetection/Src/usb_webcam_convert.c -o /tmp/usb_webcam_conversion_test
ASAN_OPTIONS=detect_leaks=0 /tmp/usb_webcam_conversion_test
```

連続取り込みのバッファ切り替え、Pipe 1/Pipe 2の組み合わせ、期限の起点は次で
ホスト試験できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/camera_capture_stream_test.c \
  Appli/FaceDetection/Src/camera_capture_stream.c \
  Appli/FaceDetection/Src/privacy_filter.c -o /tmp/camera_capture_stream_test
ASAN_OPTIONS=detect_leaks=0 /tmp/camera_capture_stream_test
```

Capture、Inference、Renderの間のキューは次で試験できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/privacy_pipeline_queue_test.c \
  Appli/FaceDetection/Src/privacy_filter.c \
  -o /tmp/privacy_pipeline_queue_test
ASAN_OPTIONS=detect_leaks=0 /tmp/privacy_pipeline_queue_test
```

480 x 480モデルでは推論約28.5 ms、NNコピーを含む合計約35 msとなり、33 ms期限を
超えることを実機で確認しました。現在の320 x 320モデルはこの結果を受けて選定した
もので、実機での推論時間は約13.3 msです。モデル選定理由と受入条件は
[ADR 0007](docs/adr/0007-select-st-yolox-320-person.md)に記録しています。
