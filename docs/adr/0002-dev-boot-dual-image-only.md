# 0002: 実行経路は dev-boot（デバッガによる Appli と FSBL の二重ロード）のみ

Status: accepted (2026-09-11)

STM32N657 には内蔵フラッシュが無く、flash-boot は署名、外部 NOR への書込み、BootROM のヘッダ解釈を伴う（旧コピーはここで詰まった）。CubeIDE の FSBL launch（Appli.elf、FSBL.elf の順にロード、AP 1、connect-under-reset、`usermain` で停止）と、VSCode の同等設定だけを提供する。FSBL は `DEBUG` 定義下で `JumpToApplication()` を先に呼び、外部メモリからのコピー（`BOOT_Application()`）を飛ばす。

## Context

特定に時間を要した事実:

- Appli 単体では何も出ない。USART1（PE5/PE6 AF7、クロック CLKP = HSI 64 MHz）と PLL は FSBL が初期化し、Appli の T-Monitor（`tm_com.c`）は USART1 のレジスタに BRR/CR1〜3 を直書きするだけで RCC も GPIO も触らない。`BRR=0x022C` は 64 MHz 前提。
- `#ifdef DEBUG` → `JumpToApplication()` は FSBL の `main.c` のユーザブロックに入っており、`.cproject` の Debug 構成が C と ASM 両方に `DEBUG` を定義することで成立している。
- 参照 zip の `.launch` はファイル名に空白を含み（`mtk3bsp2_stm32n657_FSBL Debug.launch`）、二重ロードの `loadList` は XML 属性内にエスケープされた JSON として入っている。`xmllint` では中身を検証できない。
- launch の `access_port_id=1` と `connect_under_reset` が FSBL 経路の前提。VSCode 側は `serverApID "1"`、`deviceTrustzone "Secure-only"` で対応する。

## Consequences

- dev-boot でも FSBL の `MX_XSPI2_Init` と `MX_EXTMEM_MANAGER_Init` は走る。失敗すると `Error_Handler()` に落ちて Appli は始まらない。
- 出力は `usermain` のブレークポイントで止まった後に Resume して初めて現れる。VCP を先に開く。
- flash-boot に移るなら本 ADR を再検討し、署名と Appli 側ヘッダ（FSBL は `dest+0x400` へ無条件ジャンプ）の扱いを決め直す。

## 実機での確認 (2026-09-11)

`tools/devboot_run.sh` の経路で、FSBL → Appli → μT-Kernel 初回ディスパッチ → `knl_start_device()` → `usermain` → `microT-Kernel Version 3.00` / `Start User-main program.` / `task 1` `task 2` の出力まで確認した。旧コピーが越えられなかった初回ディスパッチは、参照プロジェクトそのままの構成では問題なく通る。

この Mac (macOS 26、ST-LINK V3 FW V3J17M10) で判明した制約:

- SWD は 1000 kHz に固定する。自動 (最大) では大きな転送で libusb の pipe stall が起き、ST-LINK は USB 抜き挿しまで復帰しない。CubeIDE の `.launch` (`stlink.frequency`) と VSCode の `serverInterfaceFrequency` を 1000 にしてある。
- gdb の `load` は `set remote memory-write-packet-size 1024` (fixed) で小分けにする。
- VCP の読み出しは gdb の転送と時間を分ける。同じ USB デバイスの 2 インターフェースへ同時にバルク転送すると SWD 側が劣化する。
- `stty -f` の設定は close で破棄されるので、ボーレートは読むディスクリプタ自身で設定する (`tools/vcp_read.py`)。
