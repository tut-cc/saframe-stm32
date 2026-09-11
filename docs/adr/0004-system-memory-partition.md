# 0004: システムメモリ上端を例外スタック領域 8 KiB で区切り、newlib ヒープを分離する

Status: accepted (2026-09-11)

`CNF_EXC_STACK_SIZE` を 0 から 0x2000 にし（システムメモリ上端 0x341FE000 = `_sstack`）、リンカスクリプトで `_end` をヒープ/スタック予約の後ろへ移し、`_sbrk` を `__libc_heap_start` 〜 `__libc_heap_end`（512 B）に上限拘束する。face_detection fork の 3 ハンクの移植だが、fork 側の既存 map は参照のリンカスクリプトでリンクされており、修正は fork でも未検証だった。

## Context

- 参照プロジェクトでは、カーネルのシステムメモリが `[&_end, 0x34200000)` で、上端が MSP スタック（`_estack`）と同じ番地。`sys_start.c` は起動時に MSPLIM を `INTERNAL_RAM_START` まで下げるので、衝突をハードウェアは検出しない。
- newlib の `_sbrk` も `_end` から始まり、カーネルと同じ領域を取り合う。`_end` を動かすだけでは両者が新しい `_end` から再び衝突するので、`sysmem.c` の変更を含めて 1 つの修正になる。
- `knl_exctbl`（`.mtk_exctbl`、3392 B、1024 アライン）はリンカスクリプトに書かれていない orphan section で、`mtk3_bsp2/etc/linker/mtkernel.ld` の断片も `-T` に渡されていない。parity のため配置はリンカ任せのままにし、`readelf` で RAM 内かつ `_end` より下であることを検証する。

## Consequences

- これは静的分割であってオーバーフロー保護ではない。MSP が 8 KiB を超えるとシステムメモリを黙って壊す。
- newlib ヒープは 512 B。`printf` や `malloc` を足した時点で別の障害になる。
- リンカスクリプトの `_Min_Stack_Size = 0x2000` は低位 RAM の容量検査とセパレータであり、MSP スタックそのものではない。
