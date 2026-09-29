# 0008: 安全化済みフレームをUSB UVCへYUY2で出力する

Status: superseded in part by ADR-0009 (2026-09-29): format, resolution and conversion

## Context

ADR-0006はRAWフレームを非公開にし、安全化済みフレームだけをLCDへ公開する。
Webカメラ出力を追加しても、この公開境界をUSB側で迂回してはならない。また、LCD
表示中のRGB565バッファをUSB転送完了まで共有すると、USBホストの停止や遅延が4面
バッファの所有権を塞ぎ、Capture/Inference/Renderパイプラインを停止させ得る。

ST公式のSTM32N6 UVC Library (`uvcl`) はUSBXまたはSTM32 USB Device Library、
RTOSあり／なし、複数の映像形式を選択できる。RGB565 Frame-Based形式は変換不要
だが一般的なWebカメラアプリとの互換性が低い。H.264とMJPEGは帯域を削減できるが、
VENCまたはJPEG処理と追加の失敗経路を同時に導入する。

## Decision

USB1/CN18をUVCデバイスとして使用し、320 x 240、YUY2、10 fpsを1ストリーム公開
する。`uvcl` v3.0.1をSTM32 USB Device Library v2.11.4、RTOSなし、USB DMAなし
で使用する。

UVCへ渡すのは33 ms期限を満たしMASKまたはMOSAIC処理が完了したRGB565フレーム
だけとする。800 x 480の中央4:3領域を320 x 240へ縮小しながらUSB専用YUY2二面
バッファへコピー変換し、
USB転送完了コールバックまでその面を再利用しない。USBが未接続、停止中、または
二面とも使用中ならUSBフレームだけを破棄し、LCD公開とカメラパイプラインは止めない。

## Consequences

- USB用PSRAMを307,200 bytes追加使用する。
- RGB565からYUY2へのCPU変換は安全期限判定とLCD VBlank公開後に行うため、その
  フレームの33 ms判定値へは含めない。ただしRenderタスクを占有するため、実効fpsと
  次フレームの待ち時間は実機測定する。
- PCD割込みはμT-Kernelの`tk_def_int`で登録する。割込みハンドラではUSB IRQを
  マスクしてセマフォを通知するだけとし、HAL/UVC処理は優先度8のサービス・タスク
  で実行する。LCDのRender/Captureタスクはこれより高い優先度を維持する。
- High-Speed Isochronous転送は1 microframeあたり1 transactionとし、古典USB
  Device backendの無DMA構成で3 transactionを連続処理する負荷を避ける。
- FSBLのUSB1 HCD初期化関数は実処理が空だが、AppliのPCD初期化時にUSB1を強制
  リセットし、デバイス状態から開始する。
- USB側にはRAW、期限超過、モデル重み不正、最初の安全化完了前のフレームを渡さない。
- 30 fps、MJPEG、H.264、USB DMAは実機帯域とCPU負荷を測定した後の候補とする。

## Verification

- ホスト試験でRGB565→YUY2の原色変換と中央クロップを確認する。
- Appli DebugビルドでUVC、USB Device Core、PCD/LL USBを含めてリンクする。
- 実機でCN18の列挙、320 x 240 YUY2、10 fps、Windows/macOS/Linuxのカメラ
  アプリ互換性、1,000フレーム以上の連続動作を確認する。
- USB未接続、ホスト停止、再接続時にもLCD公開が継続し、RAW公開が0件であることを
  確認する。
