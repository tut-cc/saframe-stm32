# 0009: USB UVCを1280 x 720のVENC MJPEGへ移行する

Status: accepted (2026-09-29)

ADR-0008の映像形式、解像度、変換方式を置き換える。USB割込みの扱い、公開境界、
USB専用二面バッファの所有権はADR-0008のまま維持する。

## Context

USB UVCの目標は1280 x 720、30 fpsである。ADR-0008の構成には次の問題がある。

- 非圧縮YUY2は1280 x 720 x 2 x 30 = 約55 MB/sとなり、1 microframeあたり
  1 transactionのHigh-Speed Isochronous転送（約8.2 MB/s）に収まらない。
- 320 x 240でもCPUによるRGB565→JPEG MCU変換が最大18.6 msかかり、画素数が
  12倍の720pでは成立しない（2026-09-29実機測定）。
- ISPを通るDCMIPP出力はPipe 1（LCD用）とPipe 2（NN用）の2本だけで、720p用の
  3本目の出力はない。
- NNはセンサー中央の1944 x 1944正方形しか見ていない。センサー全幅の16:9を
  公開すると、推論されていない左右の領域を公開することになる（ADR-0006違反）。

## Decision

- Pipe 1を1280 x 720 RGB565とし、NN正方形の縦中央から切り出した16:9領域
  （1944 x 1094、縮小率約1.52）を出力する。オフセットと高さはBayer位相を保つため
  偶数にそろえる。
- 安全化（MASK/MOSAIC）はこの1280 x 720フレームに1回だけ適用し、LCDとUSBで
  共有する。NNの正規化座標は正方形内の縦オフセットを差し引いてフレーム座標へ
  変換する。
- LCDはフレーム中央800 x 450を等倍で表示する（LTDCは拡大縮小できないため）。
- USBはSTM32N6のVENC（Hantro VC8000NanoE）のJPEGモードで、RGB565フレームを
  直接MJPEGへ符号化する。旧JPEGコーデック（`HAL_JPEG`）とCPUのMCU変換は廃止する。
- VENCのEWL層は`EWL_USER_MM`と`EWL_USER_SYNC`で実装する。メモリはPSRAM上の
  64 KiB静的プールからのバンプ割り当てとし、newlibのmallocは使わない（ADR-0004）。
  同期はVENC割込みから通知するμT-Kernelセマフォで行う。
- 段階1ではUVCを15 fpsとする（スナップショット方式のキャプチャが15.6 fpsのため）。
  30 fps化はキャプチャの連続モード化と合わせて別途行う。

## Consequences

- 安全化フレームバッファは4面 x 1,843,200 bytes（約7.37 MB）になる。
  USBのJPEG出力は2面 x 256 KiB。PSRAM使用量は合計約10.1 MB（16 MB中）。
- LCDの視野はUSBより狭く、フレーム中央部分だけになる。
- USBの視野はNN正方形より上下が狭い。画面外に出たROIは見えている部分だけに
  モザイクがかかる。
- プライバシー処理とキャッシュ保守の対象画素数は480 x 480の約4倍になる。
  `filter`の実測値によっては30 fps化の前にフィルタの高速化が必要になる。
- JPEGヘッダはCPUが、スキャンデータはVENCが出力バッファへ書く。PSRAMはMPU無効の
  既定メモリマップでライトスルーになることを前提に、符号化後の無効化だけで整合を
  取る。MPUを有効化する場合はこの前提を見直す。
- VENCの符号化はRenderタスクが完了を待つ間ブロックする。公開判定とLCD公開の後に
  行うため、そのフレームの33 ms判定には含まれない。

## Verification

- Appli Debugビルドで、EWLの上書き関数（`EWLmalloc`、`EWLWaitHwRdy`など）が
  `usb_webcam_venc.o`側で採用されていることをマップファイルで確認する。
- 実機で、LCD中央800 x 450表示、USB 1280 x 720 MJPEGの表示、LCDとUSBで
  同じ位置にモザイクがかかることを確認する。
- 人が画面の上下端にいてもUSB映像でモザイクが途切れないことを確認する。
- T-Monitorで`uvc_jpeg_bytes`、`uvc_encode_max`、検出ありの`filter`、
  `max_published_total`を記録する。
