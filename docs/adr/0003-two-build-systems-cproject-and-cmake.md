# 0003: CubeIDE 管理ビルド（.cproject）と単一の CMakeLists.txt を並存させる

Status: accepted (2026-09-11)

参照の `.cproject` をそのまま使い、ルートに FSBL と Appli の 2 ターゲットを持つ `CMakeLists.txt` を 1 つ置く。CubeMX の ExternalProject super-build は再生成しない前提では無駄で、CubeIDE の CMake ネイティブ import は二重ロード launch との相性が未知のため採らない。

## Context

特定に時間を要した事実:

- `.cproject` の mtk3_bsp2 include path 4 本のうち 3 本が `${workspace_loc:/mtk3bsp2_stm32n657/Appli/mtk3_bsp2/...}` とプロジェクト名をハードコードしており、改名すると黙って壊れる。`${ProjName}` 形に直す（BSP2 マニュアルの推奨形）。改名トークンは Release の `buildPath` と `<project id>` にも残る。
- `.cproject` は `mtk3_bsp2` 配下 756 ファイルを除外なしで全部コンパイルし、他ボード向け TU は各ファイル先頭の `sys/machine.h` ガードで空になる。CMake も GLOB で同じ集合を渡す。
- `dispatch.S` はアセンブラ側に `_STM32CUBE_DISCOVERY_N657_` と 4 本の include path が無いと空オブジェクトになり、ディスパッチャが未解決になる。define は C/ASM で言語別に `.cproject` Debug と一致させる。
- HAL ソースは `Appli/Drivers`（空ディレクトリ）ではなく、`.project` の linked resources（`PARENT-1-PROJECT_LOC/Drivers/...`）でルートの `Drivers/` から引かれている。CMake のソース一覧はこの link 一覧（FSBL: HAL/LL 26 + ExtMem 9、Appli: HAL/LL 17）から導く。
- CubeIDE の Appli link は `--cmse-implib --out-implib=./secure_nsclib.o` で import library を `Appli/Debug/` に書き出す。非セキュア側の利用者が無いので CMake では生成しない。

## Consequences

- 二重管理なので、define / include / link フラグ / オブジェクト集合の parity 検査を検証項目にする。許容差（2026-09-11 の検証で確認済み、いずれも生成イメージに影響しない）:
  - CubeIDE のみ: `-fcyclomatic-complexity`（C コンパイル）、`--specs=nosys.specs`（`syscalls.c` が両側でリンクされるため無効）、`-static` と `-Wl,--start-group -lc -lm -Wl,--end-group`、CMSE import library の生成。
  - CMake のみ: `-Wl,--no-warn-rwx-segments`、link 行と `dispatch.S` のアセンブル時の `-mcmse`（TARGET_FLAGS を全言語に付けているため。text サイズは両側 53664 B で一致）。
  - 検証結果: FSBL の `.bin` は byte 単位で一致、Appli は text/data/bss とシンボル集合が一致し、差はリンク順のみ。
- `Appli/Debug/`、`FSBL/Debug/`（CubeIDE の生成物、`secure_nsclib.o` を含む）と `build/`（CMake）は gitignore。
