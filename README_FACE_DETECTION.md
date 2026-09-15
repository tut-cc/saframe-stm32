# SAFRAME — STM32N6 リアルタイム・プライバシーフィルタ

TRONプログラミングコンテスト向けのSTM32N6570-DK用プロジェクトです。計画書の
最初のマイルストーンとして、カメラ映像の顔検知と、検知領域への黒マスク／
モザイク処理を実装しています。映像と処理結果はボード上のLCDへ表示します。

## 現在できること

- IMX335カメラ映像からBlazeFace front（128 x 128、UINT8）をNeural-ARTで実行し、正面向きの顔を検出
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

## CMake / CLI / VS Code でのビルド

CubeIDE を開かずにビルドする経路も用意しています。STM32CubeIDE の
`.cproject` と CMake は並存し、生成イメージが一致することを parity 検査で
確認しています（手順と実測値は [ADR 0003](docs/adr/0003-two-build-systems-cproject-and-cmake.md)）。

必要なのは STM32CubeCLT 1.22.0 です。`/opt/ST` 以外に入れている場合は
`arm-none-eabi-gcc` を PATH に通すか、環境変数 `ARM_TOOLCHAIN_DIR` で
ツールチェーンの `bin` を指してください。

```bash
export PATH=/opt/ST/STM32CubeCLT_1.22.0/GNU-tools-for-STM32/bin:/opt/ST/STM32CubeCLT_1.22.0/Ninja/bin:$PATH
cmake --preset Debug
cmake --build --preset Debug
```

`build/Debug/mtk3bsp2_stm32n657_Appli.elf` と `..._FSBL.elf` ができます。
Debug 構成のみ対応で、Release 構成は CubeIDE 側だけです。

実機で動かすには次を使います。モデル重みの書き込みが済んでいないボードでは
初回だけ `FLASH_MODEL=1` を付けてください。

```bash
tools/devboot_run.sh 30              # 2 イメージをロードして usermain から実行
FLASH_MODEL=1 tools/devboot_run.sh   # 初回のみ、重みも書き込む
tools/flash_model.sh                 # 重みの書き込みだけ
```

VS Code はこのリポジトリのルートを開くと顔検知アプリが対象になります。
`hello_world` は `hello_world/` を単独のフォルダとして開いてください。
デバッガは ST 拡張の `stlinkgdbtarget` で、`F5` で Appli と FSBL の順に
2 イメージをロードします。

ST-LINK のデバッグクライアントは同時に 1 つだけです。CubeIDE や VS Code の
デバッグセッションを止めてから CLI を使ってください。

## テスト

画像フィルタ部分はHALやμT-Kernelに依存しないCモジュールです。PC上では次で
境界チェックを含むテストを実行できます。

```bash
cd tests
cmake --preset host-test
cmake --build --preset host-test
ctest --preset host-test
```

`tests/` はルートとは別の CMake プロジェクトです。ルートの `CMakeLists.txt` は
arm-none-eabi のクロスツールチェーンで構成されるため分けてあります。

## 現段階の制約

- 出力先はLCDのみです。USB UVCや録画出力は未実装です。
- 最大同時顔数は3です。
- 現段階は顔検出結果に基づく最新ROI描画です。計画書の最終目標である
  「同一フレームの出力保証」や異常時の全面マスクは、次段階で実装します。
- **検出できるのは正面向きの顔だけです。** 斜めを向いた顔、横顔、上下反転した顔、
  大きく傾いた顔は検出できません。BlazeFaceのfrontモデル（`blazeface_front_128`）を
  そのまま使っており、学習時の想定姿勢から外れるためです。プライバシーフィルタは
  検出できなかった顔には掛からないので、この範囲が現状の保護範囲の上限になります。
  対処としてはbackモデルの併用、入力の回転推論、検出漏れ時の全面マスクが候補です。
- フレームレートと連続動作の安定性は継続測定中です。

## 使用した既存資源

STMicroelectronicsのFace Detectionサンプルと、TRON ForumのμT-Kernel 3.0
BSP2を基礎にしています。出典、固定バージョン、ライセンスは
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)に記載しています。
