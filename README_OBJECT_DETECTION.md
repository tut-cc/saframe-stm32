# SAFRAME — STM32 Model Zoo物体検出

STM32N6570-DK上のμT-Kernel 3.0 BSP2で、カメラ画像をNeural-ARTへ入力し、
検出したperson領域をLCD上でマスクまたはモザイク化します。

COCOの `person / book / stop sign` を代理的に
`FACE / DOCUMENT / LOGO` として扱う320 x 320・3クラス版の学習・統合経路は
[modelzoo/README_PROXY_3CLASS.md](modelzoo/README_PROXY_3CLASS.md)にあります。
代理名はモデルの意味を変更するものではありません。

## 現在の固定構成

- モデル: ST-YOLOX nano `d033_w025`、320 x 320、UINT8入力／INT8出力、person 1クラス
- 生成元: STM32 AI Model Zoo Services
  `0f6210ed5156126b782e1c43249063a477484b20`、STEdgeAI Core 4.0.1／
  STM32 MCU 12.0.1、`STM32N6-GettingStarted-ObjectDetection` v2.3.0
- NPUランタイム: STAI tools 4.0.1、LL_ATON 1.1.3-dev275、
  NetworkRuntime 12.0.1（生成物と同じSTEdgeAI配布物一式）
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
  --contract modelzoo/st_yoloxn_person_320.json \
  "$MODELZOO_SERVICES_ROOT/application_code/object_detection/STM32N6"
```

importスクリプトは指定した契約ファイルに従って入力形状、入出力型、クラス数と順序、
後処理、出力テンソルに加え、生成コードとLL_ATON、STAI、静的ランタイムの世代を
検査します。不一致な生成物は既存モデルを上書きする前に拒否し、新しい重みの実行時
署名も自動生成します。`--contract`を省略した場合は、将来用の
代理3クラス契約`modelzoo/st_yoloxn_proxy3_320.json`を使用します。

YOLOv8を使う場合はYAMLの`model_type`を`yolov8n`へ変更します。YOLOv8、
YOLOv11、YOLO26は同じ`POSTPROCESS_OD_YOLO_V8_UI`経路を使うため、対応する
後処理ソースはこのリポジトリへ収録済みです。モデル変更後は必ずAppliを再ビルドし、
新しい`network_data.hex`を再書込みしてください。

## 確認済み範囲と残作業

### 安全化済みフレーム公開ゲート

この構成を選んだ理由、棄却した方式、保証する状態遷移は
[ADR 0006](docs/adr/0006-publish-only-sanitized-frames.md)に記録しています。

安全化フレームは1280 x 720のRGB565四重バッファで管理します。Pipe 1はNNが見る
センサー中央の正方形から縦中央の16:9領域を切り出すため、公開する画素はすべて
推論の対象です。LCDはこのフレームの中央800 x 450を等倍で表示します。DCMIPPは
非公開の作業バッファへsnapshot取得し、AI推論とマスク／モザイク処理が完了した
フレームだけをVBlankで表示バッファへ切り替えます。処理開始から33 msを超えたフレームは破棄され、LCDは
直前の安全化済みフレームを保持します。最初の安全化済みフレームが完成するまでは
黒画面です。

起動時はセンサーを安定して開始するため、Pipe 1で非公開バッファへ最初のsnapshotを
1枚取得して破棄します。その後にPipe 1とPipe 2のsnapshot取得を開始します。タイム
アウト時のT-Monitor出力にはPipe別の完了回数を表示するため、どちらの経路で停止した
かを判別できます。

T-Monitorには1秒ごとにcapture、AI、後処理、Vision、キャッシュinvalidate、
MASK/MOSAIC本体、キャッシュclean、Render、合計、LTDC更新、VBlank待ちの時間を
マイクロ秒単位で出力します。監視値は処理中ではなく最後に完了したフレームの値です。
公開・破棄フレーム数も同じ行へ出力します。期限判定はDWT cycle counterを使用して
33,000 usで判定します。

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

安全化済みフレームはLCDに加えて、USB1 Type-CコネクタCN18からUVC Webカメラ
として出力します。UVC形式は1280 x 720、MJPEG、15 fpsです。1280 x 720の
安全化済みRGB565フレームをVENC（ハードウェア動画エンコーダ）のJPEGモードで
USB専用二面バッファへ直接エンコードします。そのため、USB送信中のバッファが
DCMIPPに再利用されることはありません。ホストが未接続、ストリーミング停止中、
USB送信が追いつかない場合、またはエンコード結果が256 KiBの上限を超えた場合は
USBフレームだけを破棄し、LCDの公開処理は継続します。RAWフレームをUSBへ渡す
経路はありません。構成の理由は[ADR 0009](docs/adr/0009-usb-uvc-720p-venc-mjpeg.md)に
記録しています。

USBのHAL/UVC処理は割込み内ではなく、LCDのRender/Captureより低い優先度の専用
タスクで行います。USBホストがストリーミングを開始してもLCD更新を優先します。

CN18とホストをUSB Type-Cケーブルで接続し、OSのカメラアプリから`STM32 uvc`
デバイスを選択してください。T-Monitorの`uvc=streaming`、`uvc_submitted`、
`uvc_dropped`で接続状態と送信状況を、`uvc_jpeg_bytes`で直近のJPEGサイズを、
`uvc_encode_max`でVENCエンコードの最大時間を確認できます。320 x 240 MJPEG版
（旧JPEGコーデック）までは実機で確認済みです。720p VENC版の実機確認と長時間
連続動作は未確認です。30 fps（キャプチャの連続モード化）、H.264、および実際の
顔・書類・ロゴを学習したモデルは後続のマイルストーンです。

ホスト上のプライバシーフィルタ境界テストは次で実行できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/privacy_filter_test.c \
  Appli/FaceDetection/Src/privacy_filter.c -o /tmp/privacy_filter_test
ASAN_OPTIONS=detect_leaks=0 /tmp/privacy_filter_test
```

480 x 480モデルでは推論約28.5 ms、NNコピーを含む合計約35 msとなり、33 ms期限を
超えることを実機で確認しました。現在の320 x 320モデルはこの結果を受けて選定した
もので、モデル選定理由と受入条件は[ADR 0007](docs/adr/0007-select-st-yolox-320-person.md)に
記録しています。320版の実機性能は新しい重みを書き込んだ後に再測定します。
