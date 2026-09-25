# SAFRAME — YuNet顔検出

STM32N6570-DK上のμT-Kernel 3.0 BSP2で、カメラ画像をNeural-ARTへ入力し、
検出した顔領域をLCD上でマスクまたはモザイク化します。
この文書はコミット`e9b716a`時点の実装スナップショットです。
設計上の判断理由は
[ADR-0006](docs/adr/0006-publish-only-sanitized-frames.md)と
[ADR-0007](docs/adr/0007-use-yunet-for-face-privacy.md)を参照してください。

## 現在の固定構成

- モデル: YuNet（`yunetn_320_qdq_int8.onnx`）、320 x 320、
  UINT8 channel-last入力、12個のINT8出力、face 1クラス、5キーポイント
- 生成元: STM32 AI Model Zoo（`STMicroelectronics/stm32ai-modelzoo`
  `face_detection/yunet/`）のプリトレイン済みモデルと、
  `STMicroelectronics/stm32ai-modelzoo-services` の `face_detection` ユースケース
- 後処理: 候補抽出confidence 0.20、NMS 0.5、最大10件
- 時間安定化: 新規顔0.35、既存追跡0.20、IoU 0.15、ROI保持1,000 ms
  （後処理は`STM32N6-GettingStarted-FaceDetection`から移植）
- 入出力: IMX335カメラ、STM32N6570-DK LCD
- 画角: 表示PipeとNN Pipeで同じ中央正方形をクロップ（左右は表示されません）
- 実行: Vision、Render、Control/Monitorの3つのμT-Kernelタスク
- Neural-ARTランタイム: LL_ATON 1.1.3 dev275 / NetworkRuntime1201

`develop/add-modelzoo-object-detection` ブランチのST-YOLOX物体検出構成から、
モデル生成物と後処理をYuNet顔検出構成へ置き換えています。既存のカメラ、LCD、
dev-boot構成は維持しています。Neural-ARTランタイムは生成物と合わせて
ST Edge AI 4.0.1付属のLL_ATON 1.1.3 dev275 / NetworkRuntime1201へ更新しています。
移植元の`develop/add-yunet-object-detection`は実機で顔検出できた正常系ではなく、
入力をchannel-firstとして生成した未動作状態でした。このブランチでは移植元を
正常基準とせず、公式Model Zooの生成条件と隔離したGetting Startedアプリを基準に
しています。
詳しいモデル調査の経緯は
[MODEL_PROVENANCE.md](MODEL_PROVENANCE.md) を参照してください。


## 実行時データフロー

1. DCMIPP Pipe 1がLCD用RGB565を非公開の作業バッファへsnapshot取得し、
   Pipe 2が同じ中央正方形のRGB888をYuNet入力へ取得します。
2. VisionタスクがNeural-ARTで推論し、YuNetの12出力をデコードして
   confidence 0.20以上の候補にNMS 0.5を適用します。
3. 最大10顔の固定長トラッカーが検出を対応付け、平滑化・保持・拡張した
   保護ROIをRenderタスクへ発行します。
4. Renderタスクが非公開バッファ上のROIにMASKまたはMOSAICを適用します。
5. 両Pipeの取得完了から安全化完了までが33,000 us以内の場合だけ、
   LTDCのVBlankで背景アドレスを切り替えてLCDへ公開します。

RAWバッファはLTDCに設定しません。期限超過フレームは破棄し、LCDには
直前の安全化済みフレームを保持します。起動直後、最初の安全化完了前、
モデル検査失敗時は黒背景を維持します。

## タスクと同期

- **Vision**: カメラsnapshot待ち、入力診断、NPU推論、後処理、ROI追跡、
  処理結果の発行を担当します。
- **Render**: 最新の結果を受け取り、RGB565へのMASK/MOSAIC、キャッシュ同期、
  33,000 us判定、VBlank公開または破棄を担当します。
- **Control/Monitor**: B2のデバウンスとMASK/MOSAIC切替え、
  1秒周期の`FD:`/`FD-DIAG:`統計出力を担当します。

LCD背景は2面のダブルバッファで、各フレームの状態は
`WORKING -> PROCESSED -> PUBLISHED`または`WORKING -> PROCESSED -> DROPPED`と
遷移します。VisionはRenderが公開または破棄を確定するまで作業バッファを
再利用しません。カメラ完了とVision/Render間の通知にイベントフラグ、
結果とLCD操作の保護に優先度継承mutexを使います。

## ビルドと実行

1. STM32CubeIDEへルート、Appli、FSBLの3プロジェクトをimportします。
2. AppliとFSBLをDebug構成でビルドします。
3. Development modeで、YuNetのモデル重みを外部NORへ書き込みます。
   旧ST-YOLOX/BlazeFaceの重みは使えないため、このブランチへ切り替えたときは
   必ず再書き込みしてください。

```bash
export STM32N6_LOADER="<STM32CubeProgrammer>/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr"
STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -el "$STM32N6_LOADER" -hardRst \
  -w Appli/FaceDetection/Model/network_data.hex
```

4. `Appli/mtk3bsp2_stm32n657_Appli Debug.launch`を開始し、`usermain`で停止後に
   Resumeします。このlaunchはApplicationとFSBLを二重ロードし、SWDを
   1000 kHzに固定しています。T-Monitorコンソールは115200 bps、8-N-1です。
5. B2を押すとMASKとMOSAICが切り替わります。

## 安全機構と顔検出の診断

表示用RGB565 PipeとYuNet入力Pipeは、横長のカメラ画像から同じ中央正方形を
切り出します。左右の画角は失われますが、LCD上の顔とYuNetのROI座標系が一致し、
顔の縦横比も維持されます。以前の`ASPECT_RATIO_FIT`は横長画像全体を正方形へ
縮小していたため、表示の縦伸びと顔検出精度低下の原因候補でした。

起動時にはXSPI2上のモデル重みを複数の既知オフセットで署名検査します。
不一致の場合は`ERROR: program network_data.hex`を表示し、推論を開始しません。
続けてYuNetの1入力と12出力について、形式、サイズ、形状、量子化情報を
検証します。不一致の場合は`ERROR: incompatible YuNet model`を表示します。
どちらの失敗時もRAW映像を公開せず黒背景を維持します。カメラ取得が3秒以内に
完了しない場合も、最後の安全化済みフレームを保持してエラー表示します。

T-Monitorへ1秒周期で出る`FD:`ログには次の診断値が含まれます。

- `faces`: 実際にMASK/MOSAICを適用した追跡ROI数
- `detected`: 現フレームの検出で新規作成または更新したROI数
- `held`: 現フレームでは検出できず、過去の位置を保持しているROI数
- `expired`: 1,000 msを超えて失効したROI数
- `max_score`: 全アンカーの`class score * objectness`の最大値
- `candidates`: confidence 0.20を超えてNMS後に残った候補数
- `raw_candidates`: confidence 0.20を超えたNMS前の候補数

`max_score`がほぼ0の場合は、続く`FD-DIAG:`も確認します。

- `input_rgb=R[min,max,mean] ...`: 64画素ごとに採取したNN入力のRGB統計
- `hash`: 同じサンプルから算出した値。被写体を動かして値が変われば入力更新中
- `cls_q` / `obj_q`: YuNetの量子化済みclass/objectness出力の最小・最大値

RGBの各範囲が`[0,0,0]`付近、または`hash`が被写体を動かしても不変ならPipe 2
入力を調べます。入力が変化しているのに`cls_q`と`obj_q`が毎回同じなら、NPU・
重み・出力キャッシュ経路を調べます。NPU出力は後処理前にキャッシュ無効化して
から読み取ります。

中央の正面顔で`faces=0`の場合、`max_score`が0.1～0.35なら検出閾値付近、
ほぼ0または毎フレーム同一ならNN入力更新・RGB順・NPU出力を次に調べます。
`max_score > 0.35`なのに`candidates=0`なら、後処理の出力割り当てが不整合です。

### ROIの時間安定化

confidence 0.35以上の候補は新しい顔として採用し、既存ROIとIoU 0.15以上で
重なる候補はconfidence 0.20まで追跡更新に使用します。更新座標は旧位置40%、
新位置60%で平滑化します。通常ROIは左右30%、上下40%広げます。

検出が途切れたROIは1,000 ms保持し、250 msごとに余白を10ポイントずつ広げ、
最大で左右60%、上下70%とします。1,000 msを超えるとROIを解除します。今回は
全画面MASK/MOSAICへ切り替えるフェイルセーフを採用していないため、1秒を超えて
顔検出が連続して失敗した場合は顔が露出し得ます。

## モデル生成物の再生成

`Appli/FaceDetection/Model/` の`network.c`、`network_data.hex`等は、
`STMicroelectronics/STM32N6-GettingStarted-FaceDetection`の
`Model/generate-n6-model_STM32N6570-DK.sh`を参考にした
[Model/generate-n6-model_STM32N6570-DK.sh](Appli/FaceDetection/Model/generate-n6-model_STM32N6570-DK.sh)
で、ローカルの`stedgeai`から直接生成できます（STEdgeAI 4.0で生成・確認済み）。
DCMIPP Pipe 2はRGBRGB...のinterleavedデータを出力するため、生成スクリプトは
公式deploymentと同じ`--inputs-ch-position chlast`を必ず指定します。

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
最後に`verify_yunet_model_layout.py`が、320 x 320 x 3 UINT8 channel-last入力、
12個のINT8出力、量子化メタデータを検査します。個別実行も可能です。

```bash
python3 Appli/FaceDetection/Model/verify_yunet_model_layout.py
```

STEdgeAI 4.0.1では公開STAIインターフェースが`STAI_FLAG_CHANNEL_LAST`かつ
`{1, 320, 320, 3}`になる一方、生成された`network.c`内のコンパイラ内部
LL_ATON descriptorは`CHPos_First`のままです。アプリが使用する物理I/O契約は
STAI側なので、生成済み`network.c`を手修正せず、STAI flagとNHWC shapeを検査します。
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

### 公式正常系の基準

2026-09-25に、現在のリポジトリを変更しない`/tmp`隔離環境で公式
`STM32N6-GettingStarted-FaceDetection`へModel Zoo YuNetを配置し、生成と
CubeIDE ModelZoo構成のクリーンビルドを行いました。使用条件は次のとおりです。

- Getting Started commit: `f4fda657e4edcb324a9a39cb72e7d47811b06a61`
- Model Zoo Services commit: `0f6210ed5156126b782e1c43249063a477484b20`
- STEdgeAI: `4.0.1-20581`、Neural-ART compiler `1.1.3-275`
- LL_ATON: `1.1.3 dev275`、NetworkRuntime: `1201`
- STM32CubeIDE: `2.1.1`、STM32CubeProgrammer: `2.22.0`
- 生成条件: `--target stm32n6 --input-data-type uint8 --output-data-type int8 --inputs-ch-position chlast`
- ONNX SHA-256: `d03a5daad28e796dada4b3d81b5b95cdf6c02e5ab22478ae2e92aee2ed7456e2`
- `network_data.xSPI2.bin` SHA-256: `52ab70255d56fc213e1a37c08a20f098ddd01e491020da8d49149cedc8bfe90b`
- `network_data.hex`: `6e388864fbf2d8a7479403e28aae817e4effadbab6b801d4efe0b9c4fa2a309b`
- `network.c`: `da21b2ab83bb4bc74c251ac6856691004f52ddcc2f1e20d5216abba5e65b546b`
- `network_ecblobs.h`: `c6acebd4a93f82a0524df449f4f81074b0e5be70811dfa1df1ed9480a96c5a6c`
- `stai_network.c`: `744174220b2cb4972e76ea2c67eb7e74f0cffb2de10f7484b20d33a7ee58194f`
- `stai_network.h`: `644a578d575f2330890138fe63688c13ad960c78213f1e3a6250bcbabe3e2330`

公式アプリのリポジトリ同梱LL_ATONはdev262だったため、4.0.1生成物をそのまま
組み合わせると`Possible mismatch in ll_aton library used`で停止しました。
deploymentが行うのと同様にSTEdgeAI 4.0.1付属dev275とNetworkRuntime1201へ揃えて
ビルドします。実機での正面顔・confidence表示は、ボードと被写体を用いる受入試験です。

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

## 検証状況

基準コミットは`e9b716a`（`fix: stabilize YuNet face privacy detection`）です。
YuNetのchannel-last入力への修正後、実機で顔検出、約7.5～9.8 msの総処理時間、
期限超過Dropなしを確認済みです。その後に追加したROIトラッカーは、
ホスト試験とCubeIDE Debug相当のGNU Tools for STM32 14.3.rel1による
クリーンビルドまで確認済みです。

ホスト上のプライバシーフィルタ境界とYuNetスコア集計テストは次で実行できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/privacy_filter_test.c \
  Appli/FaceDetection/Src/privacy_filter.c \
  Appli/FaceDetection/Src/privacy_tracker.c -o /tmp/privacy_filter_test
ASAN_OPTIONS=detect_leaks=0 /tmp/privacy_filter_test
```

モデル入出力と量子化メタデータは次で検査できます。

```bash
python3 Appli/FaceDetection/Model/verify_yunet_model_layout.py
```

### 未検証・既知の制約

- ROIトラッカー追加後の実機でのMASK/MOSAIC追従、1,000 ms保持・失効
- トラッカー追加後の1,000フレーム連続動作、Fault/連続停止/RAW非公開の確認
- 公式単体アプリでの正面顔とconfidence表示の実機確認
- 1秒を超える連続検出漏れに対する全画面保護フォールバック

現在は最後の検出から1,000 msを超えるとROIを解除し、全画面MASK/MOSAICへは
移行しません。そのため、顔検出が1秒を超えて失敗した場合は顔が露出し得ます。
この実装は「安全化完了前のRAWフレームを公開しない」ことは保証しますが、
連続する顔検出漏れまで含む完全なプライバシー保証ではありません。

## 次の作業

1. CubeIDEのDebug構成でAppliをクリーンビルドし、
   `Appli/mtk3bsp2_stm32n657_Appli Debug.launch`で実機を起動します。
   トラッカー追加時にモデル重みは変更していないため、現在のYuNet重みが
   書き込み済みなら`network_data.hex`の再書き込みは不要です。
2. 正面、横向き、顔の移動、短時間の遮蔽、画面外への移動をMASKとMOSAICの
   両方で試します。検出が一時的に切れたフレームで
   `faces=1 detected=0 held=1`となり、ROIが消えずに拡大することを確認します。
3. 顔を完全に画面外へ移動し、最後の検出から1,000 msまでROIが保持され、
   1,000 msを超えたフレームで`expired=1 faces=0`となることを確認します。
4. 1,000フレーム以上の`FD:`ログを保存し、`total`、`detected`、`held`、
   `expired`、`published`、`dropped`、`consecutive`を評価します。受入条件は
   `total <= 33000 us`、Faultと停止なし、未加工フレームの公開なしです。
5. 基準ログを取得した後、必要な場合だけパラメータを1つずつ調整します。
   新規顔を拾わない場合は取得閾値0.35、追跡が外れる場合はIoU 0.15、
   ROIの遅れが大きい場合は旧40%/新60%の平滑化を評価し、同じ条件で比較します。
