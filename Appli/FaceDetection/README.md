# Face Detection統合

このディレクトリには、STM32N6 Getting Started Face Detectionサンプルを
既存のTRON μT-Kernelアプリケーションへ統合したコードが含まれています。

## ベースライン

- 参照元: `STM32N6-GettingStarted-FaceDetection`
- バージョン: `v1.1.0`
- コミット: `45faf18539c037a4cb766d2824041d9eba201519`
- ボード: STM32N6570-DK
- カメラ: IMX335
- モデル入力: 128 x 128 RGB、UINT8

参照元のNPUランタイム、生成済みBlazeFaceネットワーク、カメラパイプライン、
ISP設定、後処理、LCD表示、ボードサポートを使用しています。

## μT-Kernelへの統合

`Application/usermain.c`からVisionタスクを起動し、そのタスクがRenderタスクと
Control/Monitorタスクを生成します。Visionはカメラ取得、推論、後処理を行い、
検出した顔ROIをRenderへ発行します。Renderは最新の結果だけを受け取り、前景
レイヤへ黒マスクまたはモザイクを描画します。Control/Monitorはユーザーボタン
`B2`によるモード切替と、1秒周期の統計ログを担当します。

CSI/DCMIPP割り込みで通知されるカメラ取り込み完了は、μT-Kernelのイベント
フラグを使用してVisionタスクへ渡します。タスク間の検出結果はダブルバッファ、
優先度継承mutex、イベントフラグで同期します。LCD操作も専用の優先度継承
mutexで保護します。スケジューラ起動後のHAL遅延処理と時刻取得には、
μT-Kernelのシステム時刻を使用します。

参照元に含まれる`Fuse_Programming()`は意図的に呼び出していません。このため、
アプリケーションがOTP設定を自動的に変更することはありません。また、TRONの
T-MonitorがUARTを使用するため、参照元のUARTコンソール初期化も除外しています。

## ビルド

STM32CubeIDEで`mtk3bsp2_stm32n657_Appli`プロジェクトの`Debug`構成を
ビルドします。この構成では次のファイルと設定を使用します。

- リンカスクリプト: `STM32N657X0HXQ_LRUN_FACE_DETECTION.ld`
- NPUランタイムライブラリ: `FaceDetection/Vendor/Lib/NetworkRuntime1201_CM55_GCC.a`
- `FaceDetection`以下のソースファイルとインクルードパス

リンカスクリプトは、アプリケーションをSecure AXISRAM1、800 x 480の
カメラフレームバッファを外部PSRAMへ配置します。AXISRAM2からAXISRAM6は、
生成済みNeural-ARTネットワーク用のメモリとして予約します。

## モデル重みの書込み

生成済みネットワークは、XSPI2の`0x70380000`からモデル重みを読み込みます。
アプリケーションを実行する前に`Model/network_data.hex`を一度書き込んで
ください。モデルを変更した場合は、再度書込みが必要です。

```bash
export DKEL="<STM32CubeProgrammerのインストール先>/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr"
STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -el "$DKEL" -hardRst \
  -w Appli/FaceDetection/Model/network_data.hex
```

HEXファイルには書込み先アドレスが含まれています。同じデータのバイナリ形式は
`Model/network_data.xSPI2.bin`です。このバイナリを直接使用する場合は、
書込み先として`0x70380000`を明示してください。

モデル重みの書込み後は、
`Appli/mtk3bsp2_stm32n657_Appli Debug.launch`を使用してデバッグを開始します。
このlaunch構成は、`mtk3bsp2_stm32n657_FSBL.elf`と
`mtk3bsp2_stm32n657_Appli.elf`の両方をロードします。以前に別のワークスペースや
プロジェクトを実行していた場合は、Debug Configurationのロード対象を確認して
ください。STM32N6570-DKは参照元プロジェクトと同じDevelopment lifecycle状態で
使用する必要があります。

## STM32CubeIDEでのデバッグ

1. 既に別プロジェクトのデバッグセッションが動いている場合は、CubeIDEの
   `Terminate`ボタンで終了します。ST-LINK GDB Serverが残っていないことも
   確認します。
2. `File > Import... > General > Existing Projects into Workspace`を開き、
   このリポジトリのルートディレクトリを指定します。次の3プロジェクトを
   インポートします。
   - `mtk3bsp2_stm32n657`（親プロジェクト。μT-Kernelのパス解決に必要）
   - `mtk3bsp2_stm32n657_Appli`
   - `mtk3bsp2_stm32n657_FSBL`
3. 両プロジェクトで`Build Configurations > Set Active > Debug`を選択し、
   `Project > Build All`を実行します。以下の2ファイルが生成されることを
   確認します。
   - `Appli/Debug/mtk3bsp2_stm32n657_Appli.elf`
   - `FSBL/Debug/mtk3bsp2_stm32n657_FSBL.elf`
4. 初回またはモデル変更後は、先に上記の「モデル重みの書込み」手順で
   `network_data.hex`を書き込みます。通常のDebug launchではモデル重みは
   書き込まれません。
5. Project Explorerで
   `Appli/mtk3bsp2_stm32n657_Appli Debug.launch`を開き、`Debug`を押します。
   このlaunchはApplicationとFSBLの両方をロードするよう構成済みです。
6. Debug Configurationのロード対象に次の2項目があり、両方の`Download`と
   `Load Symbols`が有効であることを確認します。
   - `mtk3bsp2_stm32n657_FSBL/Debug/mtk3bsp2_stm32n657_FSBL.elf`
   - `mtk3bsp2_stm32n657_Appli/Debug/mtk3bsp2_stm32n657_Appli.elf`
7. デバッガは`usermain`で停止します。`Resume`（F8）で実行を継続します。
   顔検出処理を追う場合は、次の関数にブレークポイントを設定します。
   - `FaceDetection_Run`: タスクと初期化処理の開始
   - `FaceDetection_CameraFrameCallback`: DCMIPPパイプ2の完了
   - `stai_network_run`の呼出し行: NPU推論の開始
   - `PublishPrivacyResult`: 後処理結果の発行
   - `PrivacyRenderTask`: マスク／モザイクとLCD描画
   - `ControlMonitorTask`: B2入力と診断ログ
8. T-MonitorログはST-LINK Virtual COM PortのUSART1へ出力されます。
   CubeIDEのTerminal、または任意のシリアルターミナルを
   `115200 bps, 8-N-1, flow controlなし`で開きます。Linuxでは通常
   `/dev/ttyACM0`です。

DebugビューやGDB Consoleに別のプロジェクト名（例: `prj_stm32n6_cam`）が
表示されている場合、そのセッションではこのFace Detectionアプリは実行されて
いません。セッションを終了し、FSBL側のlaunchを選び直してください。

## 実行時診断

アプリケーションは推論開始前に、XSPI2上のモデル重みから3ワードを読み出して
期待値と比較します。LCD前景レイヤとT-Monitorには、最初に失敗した段階に応じて
次のメッセージが表示されます。

- `ERROR: program network_data.hex`: XSPI2に対応するモデル重みがありません。
- `FD: waiting for camera frame`: LCD前景レイヤは動作しており、DCMIPPパイプ2の
  画像取り込み完了を待っています。
- `ERROR: NN camera timeout`: DCMIPPパイプ2の完了割り込みを3秒以内に受信
  できませんでした。
- `FD: first inference running`: 画像取り込みが完了し、最初のNPU推論を開始します。
- `MASK | Faces N` / `MOSAIC | Faces N`: 推論結果とプライバシー処理が動作して
  います。B2を押すと2つのモードが切り替わります。
- `AI Nms | Vision Nms | Draw Nms`: 推論、推論＋後処理、描画の各所要時間です。

T-Monitorには、モデル重みの確認結果、各初期化段階、最初の推論結果、および
1秒ごとのフレーム番号、モード、検出数、処理時間も出力されます。
