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

## 追記: ルート（顔検知アプリ）への実装 (2026-09-15)

ルートに `CMakeLists.txt`、`CMakePresets.json`、`cmake/gcc-arm-none-eabi.cmake` を置き、`mtk3bsp2_stm32n657_Appli` と `mtk3bsp2_stm32n657_FSBL` の 2 ターゲットを追加した。上の Consequences は `hello_world/` での検証結果なので、ルート分をここに分けて記録する。

### hello_world から変わった点

- **最適化レベルはターゲットごとに違う。** 同じ Debug でも FSBL は `-O0`（`.cproject` に値が無く CubeIDE の既定）、Appli は `-Os`。`CMAKE_C_FLAGS_DEBUG` に `-O` を書かず `target_compile_options` で指定する。
- **`--specs=nano.specs` はコンパイル行にも必要。** CubeIDE は C と ASM の両方に渡しており、newlib-nano のヘッダ検索パスが変わる。`hello_world` の CMake はリンク時にしか付けておらず、上の許容差リストにも載っていなかった。ルートでは一致させた。
  ただし **`CMAKE_C_FLAGS` に入れてはいけない**。CMake はそれをリンク起動にも渡すため `CMAKE_EXE_LINKER_FLAGS` の分と二重になり、gcc が `attempt to rename spec 'link' to already defined spec 'nano_link'` で停止する。`target_compile_options` でコンパイル専用に付ける。
- **Appli の `.bin` は生成しない。** `STM32N657X0HXQ_LRUN_FACE_DETECTION.ld` には `0x71380000` に置かれる loadable な `.flash_section`（`*(.flash_blob)`）がある。今は空（実測 0 バイト、`.flash_blob` を使うコードが無い）だが、使われた瞬間に `objcopy -O binary` の出力が巨大化する。dev-boot は ELF をロードするので不要。FSBL の `.bin` は CubeIDE と同じく生成する。
- **Vendor のソースは明示リスト。** GLOB にすると `sourceEntries` に無い `Vendor/Utilities/Fonts` の 5 本と `isp_conf_template.c` を拾う。

### parity 検証結果 (2026-09-15)

| 照合 | 結果 |
|---|---|
| オブジェクト集合（相対パスで比較） | Appli 319、FSBL 43 とも完全一致 |
| グローバルシンボル集合 | Appli 991、FSBL 350 とも差分なし |
| グローバルシンボルのサイズ | 963 個すべて一致。片側のみのシンボルなし |
| セクションサイズ（FSBL） | アロケートされる全セクションが一致 |
| セクションサイズ（Appli） | `.rodata` +1120、`.text` −4、`.bss` −4。他は一致 |
| FSBL の `.bin` | バイト単位で完全一致 |
| NPU ランタイムの取り込みメンバ | map 上 1803 で両者一致 |
| `dispatch.S` | text 184 バイト、`knl_dispatch_entry` あり |

Appli の差は `USE_FULL_ASSERT` が埋め込む `__FILE__` 文字列の長さだけが原因。CubeIDE はプロジェクト内のソースを相対パス（`../FaceDetection/...` など 22 本）でコンパイルし、CMake は全て絶対パスで渡す。同じ 22 ファイル分の文字列長が 1190 バイトから 2290 バイトになり、差 1100 バイトがアラインメント込みで `.rodata` の +1120 として現れる。`.text` と `.bss` の −4 も同じ文字列長変化に伴うローカル定数の揺れ。生成コードそのものは、グローバルシンボル 963 個のサイズ一致が示すとおり同一。

### 許容差リストへの追加

- CMake のみ: `__FILE__` が絶対パスになること（上記）。
- 既存の許容差はそのまま有効。CubeIDE のみの `-fcyclomatic-complexity`、リンク時の `--specs=nosys.specs`、`-static`、`-Wl,--start-group -lc -lm -Wl,--end-group`、CMSE import library の生成。CMake のみの `-Wl,--no-warn-rwx-segments`、ASM とリンク行の `-mcmse`、`-Wl,--print-memory-usage`。

### headless ビルドの注意

`-import` に渡す相対パスは、シェルの作業ディレクトリではなく**ランチャ自身のディレクトリ**を基準に解決される。`hello_world/README.md` の記載どおり相対パスで実行すると `Project: file:/Applications/STM32CubeIDE.app/Contents/MacOS/FSBL can't be found!` になる。絶対パスで渡すこと。またラッパースクリプトは Java 側が失敗しても終了ステータス 0 を返すので、成功判定にはログの `Build Finished. 0 errors` を見る。

import は `Appli/.settings/language.settings.xml` と `FSBL/.settings/language.settings.xml` の `env-hash` を書き換える。差分が `env-hash` だけであることを確認してから `git restore` で戻す。
