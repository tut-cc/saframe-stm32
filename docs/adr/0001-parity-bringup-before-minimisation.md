# 0001: 参照プロジェクトとの parity bring-up を最小化より先に行う

Status: accepted (2026-09-11)

`hello_world/` は TRON Forum 配布 zip（`mtk3bsp2_stm32n657`）の verbatim コピーを基にし、FSBL の周辺初期化、Appli の ADC/I2C 初期化、`mtk3_bsp2` の未使用ツリー（`ra_fsp` / `nxp_mcux` / `xmc_mtb` 等）を削らない。参照との差分をメモリ分割修正（ADR 0004）とビルド定義（ADR 0003）だけに絞ることで、実機で動かなかったときに原因を帰属できる。

## Considered Options

- `mtk3bsp2_stm32n657_face_detection` fork: Makefile 付きだが、define が `.cproject` と食い違っている（`USE_HAL_DRIVER` 欠落、死んだ `VECT_TAB_SRAM`）。
- 旧コピー `stm32n6570-dk/saframe-stm32`: Make 化・物理剪定・flash-boot 化を同時に行い、初回ディスパッチで LOCKUP（PC=0xEFFFFFFE、CFSR=0x00020001）して切り分け不能になった。7 コミットは未 push でローカルにしか無い。
- 真の最小構成（UART + GPIO + カーネルのみ）: 参照との差分が大きく、動かないときに参照の実績が使えない。

## Consequences

- ファイル数は 949。最小化は実機で動いた後の第2段階とする。
- 参照の Release 構成は `Application` と `mtk3_bsp2` を含まず壊れているので、Debug のみサポートと明記する。
- `.ioc` から再生成しない。`.cproject` の include path と `main.c` のユーザブロックが上書きされる。
