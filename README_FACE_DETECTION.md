# SAFRAME — STM32N6 リアルタイム・プライバシーフィルタ

> この文書はBlazeFace版の記録です。現在のModel Zoo物体検出版は
> [README_OBJECT_DETECTION.md](README_OBJECT_DETECTION.md)を参照してください。

TRONプログラミングコンテスト向けのSTM32N6570-DK用プロジェクトです。計画書の
最初のマイルストーンとして、カメラ映像の顔検知と、検知領域への黒マスク／
モザイク処理を実装しています。映像と処理結果はボード上のLCDへ表示します。

## 現在できること

- IMX335カメラ映像からBlazeFace（128 x 128、UINT8）をNeural-ARTで実行
- 顔領域を15%拡張し、LCDの表示範囲内へクリップ
- 黒マスク、または16 x 16画素単位のモザイクをARGB4444前景レイヤへ描画
- STM32N6570-DKの青いユーザーボタン `B2` でモードを切替
- μT-Kernel 3.0のタスク、イベントフラグ、優先度継承mutexで処理を分離
- T-Monitorへフレーム番号、検出数、AI／画像処理／描画時間を1秒ごとに出力

起動時のモードは `MASK` です。ボタンを押すたびに `MASK` と `MOSAIC` が
切り替わります。

## ソフトウェア構成

| μT-Kernelタスク | 優先度 | 役割 |
|---|---:|---|
| Vision | 5 | カメラ取得、NPU推論、後処理、顔ROIの発行 |
| Render | 8 | 最新ROIのマスク／モザイク描画、LCD反映 |
| Control/Monitor | 12 | B2のポーリングとデバウンス、統計ログ |

DCMIPPのフレーム完了は割り込みからイベントフラグでVisionタスクへ通知します。
VisionとRenderの受け渡しはダブルバッファ化した検出結果だけに限定し、Renderが
遅れた場合は古い結果を溜めず、最新結果を描画します。LCDと結果共有部は別々の
優先度継承mutexで保護しています。

## 採用した開発環境

STM32CubeIDEを採用しています。ST公式サンプル、CubeMX設定、FSBL、デバッガの
構成をそのまま活用でき、コンテスト提出後も再現しやすいためです。

- STM32CubeIDE 2.1.1
- GNU Tools for STM32 14.3.rel1
- STM32CubeProgrammer 2.22.0
- STM32N6570-DK（Development lifecycle）

## 最短のビルド／実行手順

1. STM32CubeIDEで `File > Import... > General > Existing Projects into Workspace`
   を開き、このリポジトリのルートを指定します。
2. 親プロジェクト `mtk3bsp2_stm32n657`、`mtk3bsp2_stm32n657_Appli`、
   `mtk3bsp2_stm32n657_FSBL` の3つをインポートします。親プロジェクトは
   μT-Kernelのインクルードパス解決に必要です。
3. AppliとFSBLを `Debug` 構成にして `Project > Build All` を実行します。
4. 初回だけ、次の要領でモデル重みを外部Flashへ書き込みます。

```bash
export STM32N6_LOADER="<STM32CubeProgrammer>/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr"
STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -el "$STM32N6_LOADER" -hardRst \
  -w Appli/FaceDetection/Model/network_data.hex
```

5. `Appli/mtk3bsp2_stm32n657_Appli Debug.launch` からデバッグを開始します。
   この構成はFSBLとApplicationの両方をロードします。`F8`で `usermain` から
   実行を再開します。
6. 必要ならST-LINK Virtual COM Portを `115200 bps, 8-N-1` で開きます。

詳細な書込み、デバッグ、診断方法は
[Face Detection統合手順](Appli/FaceDetection/README.md)を参照してください。

## テスト

画像フィルタ部分はHALやμT-Kernelに依存しないCモジュールです。PC上では次で
境界チェックを含むテストを実行できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/privacy_filter_test.c \
  Appli/FaceDetection/Src/privacy_filter.c -o /tmp/privacy_filter_test
ASAN_OPTIONS=detect_leaks=0 /tmp/privacy_filter_test
```

連続撮影のバッファ切り替えとPipe 1/Pipe 2の組み合わせも同様に試験できます。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -IAppli/FaceDetection/Inc tests/camera_capture_stream_test.c \
  Appli/FaceDetection/Src/camera_capture_stream.c \
  Appli/FaceDetection/Src/privacy_filter.c -o /tmp/camera_capture_stream_test
ASAN_OPTIONS=detect_leaks=0 /tmp/camera_capture_stream_test
```

## 現段階の制約

- 出力先はLCDのみです。USB UVCや録画出力は未実装です。
- 最大同時顔数は3です。
- 現段階は顔検出結果に基づく最新ROI描画です。計画書の最終目標である
  「同一フレームの出力保証」や異常時の全面マスクは、次段階で実装します。
- 実機のカメラ／NPU／LCD連続動作とフレームレートは実機確認が必要です。

## 使用した既存資源

STMicroelectronicsのFace Detectionサンプルと、TRON ForumのμT-Kernel 3.0
BSP2を基礎にしています。出典、固定バージョン、ライセンスは
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)に記載しています。
