# saframe_hello_world (μT-Kernel 3.0 / STM32N6570-DK)

STM32N6570-DK 上で μT-Kernel 3.0 (BSP2) を起動し、シリアルにメッセージを出しつつ LED 2 個を点滅させる動作確認用の構成です。

## 位置づけ

- TRON Forum 配布の `mtk3bsp2_stm32n657` を**ほぼそのまま**持ち込んだ「素性の分かる動作確認用」プロジェクトです (pristine BSP parity bring-up)。
- 目的は「まず実機で出力が出ること」で、不要ペリフェラルや未使用 BSP ツリーの削減 (minimisation) は次フェーズで行います。今この段階で削ると、不具合が移植由来か削減由来か切り分けできなくなるためです。
- 起動形態は開発ブート (dev-boot) のみです。デバッガが Appli と FSBL を AXISRAM に転送し、FSBL が `JumpToApplication()` で Appli に制御を渡します。

## ライセンスと改変点

このツリーは TRON Forum の `mtk3bsp2_stm32n657` (T-License 2.2) をそのまま複製したものです。プロジェクト名の変更 (`.project` / `.cproject` / `.launch` / `.ioc` 内のトークン置換) を除き、内容を変更したソースは次の 3 ファイルだけです。

- `Appli/STM32N657X0HXQ_LRUN.ld`
- `Appli/Core/Src/sysmem.c`
- `Appli/mtk3_bsp2/config/config.h`

この 3 つは「μT-Kernel の Imalloc 領域と MSP スタックの分離」という 1 つの修正を構成します。各ファイル内のコメントを参照してください。T-License 2.2 の条件はツリー内の原文に従います。

## 構成

- `Appli/` : μT-Kernel 本体 (`mtk3_bsp2/`) とユーザコード (`Application/usermain.c`)
- `FSBL/` : クロック、USART1 (PE5/PE6)、VddIO3、XSPI2/ExtMem の初期化を行う一次ブートローダ
- `CMakeLists.txt` / `CMakePresets.json` / `cmake/` : CLI と VSCode 用のビルド定義
- `.vscode/` : CMake Tools とデバッグ設定
- `tools/` : コマンドラインからの実機実行 (`devboot_run.sh`) と VCP リーダ (`vcp_read.py`)

## ビルド方法 (3 通り)

### 1. コマンドライン (CMake)

STM32CubeCLT 1.22.0 の `CMake/bin`、`Ninja/bin`、`GNU-tools-for-STM32/bin` を PATH に通した状態で、`hello_world/` 直下から実行します。`cmake` と `ninja` は PATH 上のものを使います。`arm-none-eabi-gcc` だけは PATH に無くても `/opt/ST/STM32CubeCLT_*` 以下を自動探索します。

```
cmake --preset Debug
cmake --build --preset Debug
```

生成物は `build/Debug/saframe_hello_world_Appli.elf` と `build/Debug/saframe_hello_world_FSBL.elf` です。

### 2. VSCode

- `hello_world/` をワークスペースとして開きます (リポジトリ直下を開いた場合は、直下の `.vscode/` が同じ内容で `hello_world/` を指します)。
- 拡張機能は ST の STM32 VSCode Extension のみを使います (clangd と CMake Tools が同梱)。
- コンパイラ、Ninja、CMake のパスは `.vscode/settings.json` に固定済みです。GUI から起動した VSCode がシェルの PATH を継承しないための措置です。
- CMake Tools でプリセット `Debug` を選び、ビルドします。

### 3. STM32CubeIDE

1. File → Import → General → Existing Projects into Workspace
2. ルートに `hello_world` を指定し、**Search for nested projects** を有効にします。
3. `saframe_hello_world` / `saframe_hello_world_Appli` / `saframe_hello_world_FSBL` の**3 つすべて**をインポートします (ルートはコンテナで、サブプロジェクトを自動では引き連れません)。
4. `_Appli` → `_FSBL` の順にビルドします。
5. ビルド構成は **Debug のみ**サポートします。Release は配布時点からソース指定とマクロ定義が欠けているため動きません。

## 実行手順 (実機)

### 0. コマンドライン (検証済み: 2026-09-11)

```
cd hello_world
tools/devboot_run.sh 30
```

ST-LINK_gdbserver を SWD 1 MHz で起動し、gdb が Appli.elf、FSBL.elf の順に 1 KiB パケットで AXISRAM へ転送、FSBL の `main` で一旦停止してから VCP リーダ (`tools/vcp_read.py`) を開き、`usermain` まで走らせて detach、その後 30 秒間の出力を `build/Debug/vcp.log` に記録します (gdbserver のログも `build/Debug/` に出ます)。この順序には理由があります (下の「macOS での注意」)。

基板が前回の実行から走り続けていた場合、ST-LINK 内の FIFO に残っていた `task 1`/`task 2` がログ先頭に出てから、リセット後のバナーが続きます。

1. ボードを開発ブート (dev-boot) に設定します。BOOT スイッチの具体的な位置は本 BSP に記載がありません。**UM3363 を参照し、ボード上のシルク表記で確認してください**(推測で設定しないこと)。
2. USB を接続し、**先にシリアルポートを開きます**。ST-LINK の VCP は macOS では `/dev/cu.usbmodem*`、**115200 8N1** です。先に開いておかないと初期出力を取りこぼします。
3. STM32CubeIDE で `saframe_hello_world_FSBL Debug` のデバッグ構成を起動します。Appli → FSBL の順に転送され、**`usermain` で停止します**。
4. **Resume を押すまで何も出力されません。** Resume 後に下記の出力が始まります。
5. VSCode の F5 (`saframe_hello_world dev-boot (Appli + FSBL)`) も実機で確認済みです (2026-09-11: 2 イメージ転送、`usermain` 停止、Resume 後に VCP へ出力)。ST 拡張のスニペット設定 (`get-projects-binary-from-context1` を使うもの) は CubeMX 生成プロジェクト専用で、この構成では `project settings not found` で止まるため使いません。
7. デバッグクライアントは **同時に 1 つだけ** にしてください。VSCode のセッション、CubeIDE、`tools/devboot_run.sh` のどれかが ST-LINK を掴んでいると、他は `Device connect error` や `Target USB comms error` で失敗します。`Target USB comms error` が出た後は USB の抜き挿しが必要です。
6. CubeIDE / VSCode の両 launch は SWD 周波数を **1000 kHz** に固定してあります (自動/最大では下記の stall が起きます)。

### macOS での注意 (ST-LINK V3、macOS 26 で確認)

- **USB ポートで結果が変わります。** この Mac では左側の USB-C ポート (`/dev/cu.usbmodem1102` として見える側) では安定し、右側 (`usbmodem3102`) では列挙だけで libusb の `pipe is stalled` が出ました。stall が続くときはまずポートを変えてください。
- SWD を自動 (最大) 周波数にすると、数十 KB の転送中に libusb の `pipe is stalled` で ST-LINK が固まります。固まった後は **USB ケーブルの抜き挿し** が必要です。1000 kHz と 1 KiB パケットで安定します。
- `stty -f` で設定したボーレートはデバイスを閉じた時点で破棄されます。次に `cat` で開くと 9600 bps になり、ST-LINK はその line coding を UART に反映して全バイトを捨てます。`tools/vcp_read.py` は同じディスクリプタで 115200 に設定してから読みます (`screen` / `minicom` でも可)。
- gdb の転送中に VCP を読み続けると、同じ USB デバイス上の SWD 転送が 10 倍遅くなるか停止します。`devboot_run.sh` は転送完了後にリーダを開きます。

### 期待される出力

```


microT-Kernel Version 3.00

Start User-main program.
task 1
task 2
task 1
task 2
...
```

バナー (`microT-Kernel Version 3.00`、前後に空行) が先頭、次に `Start User-main program.`、以降 `task 1` と `task 2` が交互に流れます。同時に LED が点滅します。

- PO1 : 500 ms ごとにトグル
- PG10 : 700 ms ごとにトグル

### 何も出力されないとき

まず FSBL が `Error_Handler()` ではなく `JumpToApplication()` に到達しているかを確認し、次にデバッガで USART1 の `CR1` / `BRR` / `ISR` と PE5/PE6 の AF 設定を見てください。カーネル側を疑うのはその後です。

## 制限事項と既知の注意点

- Release 構成は非サポートです (Debug のみ)。
- フラッシュブートおよび署名付きイメージには対応していません。開発ブート専用です。
- USART1 は FSBL が初期化します。Appli 側の `tm_com.c` は BRR と CR1-3 を書くだけで、未初期化なら TXE 待ちで無限に止まります。
- `UART_BRR=0x022C` は CLKP = HSI 64 MHz を前提にしています。クロック構成を変えるとボーレートがずれます。
- newlib のヒープは 512 B しかありません。T-Monitor の出力に足りる量です。
- MSP のオーバーフローはハードウェアで保護されません。`sys_start.c` が MSPLIM を下げるため、8 KiB を超えると Imalloc 領域を静かに壊します。上記の修正は「保護」ではなく「静的な領域分割」です。
- `.ioc` からの再生成は禁止です。`.cproject` のインクルードパスと `main.c` のユーザブロックが壊れます。
- CMake ビルドは CMSE インポートライブラリを生成しません (非セキュア側の利用者がいないため)。CubeIDE ビルドでは `Appli/Debug/secure_nsclib.o` として生成されます。
- 実機動作は 2026-09-11 に `tools/devboot_run.sh` と VSCode F5 の両経路で確認済みです (バナー、`Start User-main program.`、`task 1`/`task 2` が 50 秒で 97 回/70 回)。CubeIDE GUI からのデバッグ起動は未確認です。
- CubeIDE のヘッドレスビルド (GUI なしの一括ビルド) は次のコマンドで確認済みです (CubeIDE 2.1.1、両 ELF 生成、0 errors)。

```
/Applications/STM32CubeIDE.app/Contents/MacOS/STM32CubeIDE --launcher.suppressErrors -nosplash \
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data <空のワークスペース> \
  -import hello_world -import hello_world/FSBL -import hello_world/Appli \
  -build saframe_hello_world_Appli/Debug -build saframe_hello_world_FSBL/Debug
```

  インポートは `Appli/.settings/language.settings.xml` と `FSBL/.settings/language.settings.xml` の `env-hash` を書き換えます。コミット前に差分を確認してください。
