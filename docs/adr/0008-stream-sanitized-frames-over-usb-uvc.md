# 0008: 安全化済みフレームをH.264 UVCで30 fps出力する

Status: accepted (2026-09-29、YUY2/MJPEG構成を置換)

## Context

ADR-0006の公開境界をUSBでも維持しながら、LCDと推論を止めずに公称30 fpsを
提供する必要がある。CPUによる色変換とJPEG同期エンコードではRenderタスクの占有が
大きく、実測は約10 fpsだった。STM32N6のVENCを利用するST公式サンプルはH.264を
30 fpsで生成できるが、FreeRTOS/USBXをそのまま導入すると既存構成への変更が大きい。

## Decision

USB1/CN18はH.264 Frame-Based UVC専用とし、320 x 240、30 fps、Annex-B、約1 Mbps
VBR、GOP 30を公開する。VENC/H.264 API/EWL/LL VENCだけをST公式
`x-cube-n6-ai-h264-usb-uvc`の固定コミットから移植し、μT-Kernelと既存のSTM32 USB
Device Libraryを維持する。

Renderタスクは安全化とLCD公開が完了した800 x 480 RGB565画像の中央640 x 480を
2画素おきにUSB専用320 x 240二面入力へコピーして直ちに戻る。VENC専用タスクは
優先度7（Render/Captureより低くUSBサービスの優先度8より高い）で33/33/34 ms周期に
動く。新しい安全画像がない周期は直近の安全画像を再エンコードする。起動時は黒画像
を用い、RAW画像、期限超過画像、モデル不正時の画像へフォールバックしない。

H.264出力も二面化する。ホストが両面を保持していればその周期を破棄し、次の成功
フレームをIDRとしてSPS/PPSを前置する。接続・再接続時にも同じ再同期を行う。

## Consequences

- 入力307,200 bytes、VENC linear pool 4 MiBとsoftware pool 512 KiBをPSRAMに
  追加する。VENCが書き込みUSB Device Libraryが読む最大出力262,144 bytesは、
  公式サンプルと同様にMPUで非キャッシュ化した内蔵AXISRAM1へ配置する。
- VENC、VENCRAMのクロック/RIFとVENC割込みを有効化する。EWLの完了待ちは
  μT-Kernelセマフォへ接続する。
- UVC出力30 fpsは異なるカメラ画像30枚/秒を意味しない。`published_fps`が低い場合は
  `uvc_repeated`が増える。
- 対応ホストはLinuxのVLC/guvcview、およびWindowsのFFmpeg `ffplay`に限定する。
  MJPEGのみを扱うアプリは対象外とする。

## Verification

- Debugビルドを警告増加なしで完了し、中央クロップ/縮小をホスト試験する。
- Annex-B出力のSPS/PPS/IDR/Pフレームを`ffprobe`で確認する。
- 実機10分試験で平均29.5 fps以上、1秒区間28 fps以上、USBバッファ枯渇0、LCD停止0、
  Fault 0を確認する。
- USB切断・再接続、MASK/MOSAIC、モデル不正、期限超過でRAWが公開されず、IDRから
  再開することを確認する。

## Post-implementation review

2026-09-29にST公式資料と固定元の`v2.2.1`サンプルに対し、移植後の
構成を再照合した。次の点は公式の推奨と一致する。

- RGB565入力、frame mode、H.264 byte stream、picture/MB rate controlの利用。
- カメラフレームとVENC参照バッファは外部PSRAM、H.264出力は非キャッシュの
  内蔵SRAMに配置する。
- VENC RIMC/RISC、クロック、LL VENC初期化、割り込み完了待ちをOSに接続する。
- `UVCL_PAYLOAD_FB_H264`とimmediate modeを使い、`UVCL_ShowFrame()`後は
  `frame_release`までバッファを再利用しない。

公式サンプルとの意図的な差分は、FreeRTOS/USBXの代わりにμT-Kernel/
STM32 USB Device Libraryを維持すること、およびSPS/PPSを全IDRで自動挿入せず、
開始・再接続・ドロップ復帰時に明示的に付与することである。いずれも現行の
EWL/UVCL APIの契約内である。
