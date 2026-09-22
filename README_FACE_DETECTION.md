# SAFRAME — YuNet顔検出

STM32N6570-DK上のμT-Kernel 3.0 BSP2で、カメラ画像をNeural-ARTへ入力し、
検出した顔領域をLCD上でマスクまたはモザイク化します。

## 現在の固定構成

- モデル: YuNet（`yunetn_320_qdq_int8.onnx`）、320 x 320、UINT8入力／INT8出力、
  face 1クラス、5キーポイント
- 生成元: STM32 AI Model Zoo（`STMicroelectronics/stm32ai-modelzoo`
  `face_detection/yunet/`）のプリトレイン済みモデルと、
  `STMicroelectronics/stm32ai-modelzoo-services` の `face_detection` ユースケース
- 後処理: confidence 0.5、NMS 0.5、最大10件（`Appli/FaceDetection/Vendor/Postprocess/Src/fd_pp_yunet.c`、
  `STMicroelectronics/STM32N6-GettingStarted-FaceDetection` から移植）
- 入出力: IMX335カメラ、STM32N6570-DK LCD
- 実行: Vision、Render、Control/Monitorの3つのμT-Kernelタスク

`develop/add-modelzoo-object-detection` ブランチのST-YOLOX物体検出構成から、
モデル生成物と後処理をYuNet顔検出構成へ置き換えています。既存のカメラ、LCD、
Neural-ARTランタイム、dev-boot構成は維持しています。詳しいモデル調査の経緯は
[MODEL_PROVENANCE.md](MODEL_PROVENANCE.md) を参照してください。

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

## モデル生成物の再生成

`Appli/FaceDetection/Model/` の`network.c`、`network_data.hex`等は、
`STMicroelectronics/STM32N6-GettingStarted-FaceDetection`の
`Model/generate-n6-model_STM32N6570-DK.sh`を参考にした
[Model/generate-n6-model_STM32N6570-DK.sh](Appli/FaceDetection/Model/generate-n6-model_STM32N6570-DK.sh)
で、ローカルの`stedgeai`から直接生成できます（STEdgeAI 4.0で生成・確認済み）。

```bash
Appli/FaceDetection/Model/generate-n6-model_STM32N6570-DK.sh \
  <path-to>/yunetn_320_qdq_int8.onnx
```

`yunetn_320_qdq_int8.onnx`は
`STMicroelectronics/stm32ai-modelzoo`の
`face_detection/yunet/Public_pretrainedmodel_public_dataset/widerface/yunetn_320/`
から取得します（Git LFS管理）。生成には`stedgeai`と`arm-none-eabi-objcopy`が
PATHに必要です。スクリプトは`Model/`直下の`network.c`、`network_ecblobs.h`、
`stai_network.c/h`、`network_data.xSPI2.bin`、`network_data.hex`を上書きします。
同ディレクトリの`user_neuralart_STM32N6570-DK.json`と
`my_mpools/stm32n6-app2_STM32N6570-DK.mpool`はSTM32N6570-DKボード固定の
Neural-ART設定で、モデルに依存せず流用できます。

再生成後は、`Appli/FaceDetection/Src/face_detection_app.c`の
`NetworkWeightsValid()`にある署名テーブル（`network_data.hex`先頭の
既知オフセットの値）を新しい`network_data.hex`から採り直してください。

生成された`Model/stai_network.h`から`AI_FD_YUNET_PP_IMG_SIZE`（320）、
`AI_FD_YUNET_PP_NB_KEYPOINTS`（5）、`AI_FD_YUNET_PP_OUT_{32,16,8}_NB_BOXES`
（100 / 400 / 1600、`STAI_NETWORK_OUT_*`の出力形状から算出）は`app_config.h`に
反映済みです。

`Vendor/Postprocess/Inc/fd_yunet_anchors_{32,16,8}.h`（グリッドセルのアンカー
座標テーブル）は、本来`stm32ai-modelzoo-services`の`face_detection`
デプロイパイプラインが生成するファイルですが、その生成アルゴリズム
（`generate_yunet_anchor()`、入力サイズとstride列だけで決まる純粋なグリッド
計算）を [tools/generate_yunet_anchors.py](tools/generate_yunet_anchors.py) として
再実装し、Pythonパイプライン自体は動かさずに直接生成・配置済みです
（依存パッケージ・Cコンパイラ・実機は不要。詳細は
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 参照）。
モデル自体を差し替える場合は、次で再生成してください。

```bash
python3 tools/generate_yunet_anchors.py
```

（`IMG_SIZE`/`STRIDES`/`NB_BOXES`はスクリプト先頭にハードコードしているため、
入力解像度が変わるモデルに差し替える場合はそこも合わせて修正してください。）

`stm32ai-modelzoo-services`の完全なPythonデプロイパイプライン
（[modelzoo/deployment_n6_face_detection.yaml](modelzoo/deployment_n6_face_detection.yaml)、
[tools/import_modelzoo_face_detection.sh](tools/import_modelzoo_face_detection.sh)）も
代替手段として残していますが、こちらは重いPython依存関係（hydra/omegaconf/
onnxruntime等）に加え、実機（STM32N6570-DK、ST-Link接続）への書き込みまで
一括で行う想定のもので、アンカーヘッダだけが目的なら不要です。

## 確認済み範囲と残作業

ホスト上のプライバシーフィルタ境界テストは次で実行できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/privacy_filter_test.c \
  Appli/FaceDetection/Src/privacy_filter.c -o /tmp/privacy_filter_test
ASAN_OPTIONS=detect_leaks=0 /tmp/privacy_filter_test
```

このセッションでは以下を実装済みです:
- `fd_pp_yunet.c` / `app_postprocess_fd_yunet_ui.c`
  （`STM32N6-GettingStarted-FaceDetection` から移植した後処理実装）
- `face_detection_app.c` / `privacy_filter.c` / `privacy_filter.h` の
  `fd_pp_out_t`（顔検出・キーポイント対応）ベースへの置き換え
- `app_config.h` の `POSTPROCESS_FD_YUNET_UI` への切り替え
- `Appli/FaceDetection/Model/` 配下の実際のNeural-ART生成物
  （`stedgeai generate`をローカル実行して生成。`network.c`、`network_data.hex`等）
  と、`NetworkWeightsValid()`の署名テーブル更新
- `Vendor/Postprocess/Inc/fd_yunet_anchors_{32,16,8}.h`（アンカーテーブル）の生成
  （`tools/generate_yunet_anchors.py`。`fd_pp_yunet.c`と
  `app_postprocess_fd_yunet_ui.c`がこのヘッダを含めて実際にコンパイルできることを
  `arm-none-eabi-gcc -fsyntax-only`で確認済み）

未着手・未確認の項目:
- 実機ビルド（STM32CubeIDEでのフルビルド）・実機での連続動作、フレームレート、
  320 x 320モデルの推論時間（このサンドボックス環境には物理ボードが無いため
  未実施）
