# 0004: システムメモリ上端を例外スタック領域 4 KiB で区切り、newlib ヒープを分離する

Status: accepted (2026-09-11)、改訂 (2026-09-29, #5)

`CNF_EXC_STACK_SIZE` を 0 から 0x1000 にし（システムメモリ上端 0x340FF000 = `_sstack`）、リンカスクリプトで `_end` をヒープ/スタック予約の後ろへ移し、`_sbrk` を `__libc_heap_start` 〜 `__libc_heap_end`（512 B）に上限拘束する。face_detection fork の 3 ハンクの移植だが、fork 側の既存 map は参照のリンカスクリプトでリンクされており、修正は fork でも未検証だった。

加えて stm32_cube 用 `config_bsp.h` で `USE_SPMON` を有効にし、`dispatch.S` の MSPLIM 追従でスタック溢れをハードウェアで検出する。

## Context

- 現行の Appli は `STM32N657X0HXQ_LRUN_FACE_DETECTION.ld` でリンクされ、RAM は AXISRAM1 `[0x34000400, 0x34100000)`、`_estack = 0x34100000`（= `CNF_SYSTEMAREA_END`）、`_Min_Stack_Size = 0x1000`。hello_world 時点の値（RAM 上端 0x34200000、`_sstack = 0x341FE000`、0x2000）から変わっている。
- 参照プロジェクトでは、カーネルのシステムメモリの上端が MSP スタック（`_estack`）と同じ番地だった。`sys_start.c` は起動時に MSPLIM を `INTERNAL_RAM_START` まで下げるので、衝突をハードウェアは検出しない。
- newlib の `_sbrk` も `_end` から始まり、カーネルと同じ領域を取り合う。`_end` を動かすだけでは両者が新しい `_end` から再び衝突するので、`sysmem.c` の変更を含めて 1 つの修正になる。
- FaceDetection 構成への移行時に、リンカスクリプトと `sysmem.c` の 2 ハンクが CubeIDE 既定に戻り、newlib ヒープと Imalloc が再び重なっていた（#5）。
- μT-Kernel の armv8m ポートはタスクも MSP で走らせる。MSPLIM をタスクごとに張り替える処理は `dispatch.S` の `#if USE_SPMON` 内にあり、stm32_cube 用 `config_bsp.h` はこれを定義していなかったため、MSPLIM は起動から停止まで `INTERNAL_RAM_START` のままだった。
- `knl_exctbl`（`.mtk_exctbl`、3392 B、1024 アライン）はリンカスクリプトに書かれていない orphan section で、`mtk3_bsp2/etc/linker/mtkernel.ld` の断片も `-T` に渡されていない。parity のため配置はリンカ任せのままにし、`readelf` で RAM 内かつ `_end` より下であることを検証する。

## Consequences

- `USE_SPMON` 有効時は `sys_start.c` で MSPLIM を下げず、`knl_main()` 中も起動スタックは `_sstack` で保護される。初回ディスパッチ以降は `knl_tmp_stack` またはタスクスタックの下端が MSPLIM になり、溢れると UsageFault（CFSR.STKOF）になる。障害記録への到達はカーネルの例外ベクタ側の問題として別に扱う。
- 割込みは実行中タスクのスタック上に積まれるので、各タスクの `stksz` はネストした割込み分を含む必要がある。タスクが無い間は `knl_tmp_stack` 上で割込みが走るため、`CNF_TMP_STACK_SIZE` を 1024 B にした。
- newlib ヒープは 512 B。浮動小数点書式の `printf` や `malloc` を足した時点で ENOMEM になる（Imalloc は壊さない）。
- リンカスクリプトの `_Min_Stack_Size = 0x1000` は低位 RAM の容量検査とセパレータを兼ねるが、MSP スタックそのものは `[_sstack, _estack)` である。
- CubeIDE 既定の `/DISCARD/ { libc.a(*) ... }` はアーカイブのフルパスと一致せず何も捨てていなかった（正しく一致させると libc 全体が消えてリンクできない）ので削除した。
