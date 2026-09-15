# saframe-stm32

STM32N6570-DK 上で μT-Kernel 3.0 BSP2 を動かす、TRON プログラミングコンテスト 2026 向けのリポジトリ。`hello_world/` は TRON Forum の参照プロジェクトと等価な構成で起動を確認するためのサンプル。

## Language

### ブートとイメージ

**FSBL**:
First Stage Boot Loader。BootROM の次に実行され、クロック・外部メモリ・デバッグ用シリアルを初期化してから Appli に制御を渡すイメージ。
_Avoid_: ブートローダ、第1段ローダ、bootloader

**Appli**:
μT-Kernel とユーザプログラムを含むアプリケーションイメージ。FSBL から制御を受け取って動く。
_Avoid_: アプリ、app、ファームウェア

**dev-boot**:
デバッガが FSBL と Appli を内部 SRAM に直接ロードして起動する開発用の起動経路。署名も外部フラッシュへの書込みも伴わない。
_Avoid_: RAM ブート、デバッグブート、development boot

**flash-boot**:
署名済みの FSBL を外部 NOR フラッシュから BootROM が起動する経路。このリポジトリでは扱わない。
_Avoid_: 本番ブート、XSPI ブート、通常起動

**LRUN**:
Load and Run。イメージを内部 SRAM にコピーしてから実行する配置方式。
_Avoid_: ロード実行、RAM 実行

### カーネル

**BSP2**:
TRON Forum が配布する μT-Kernel 3.0 のボードサポートパッケージ第2版。ベンダ SDK（STM32Cube）の上に載る。
_Avoid_: BSP、ポート、mtk3_bsp2（ディレクトリ名として以外）

**usermain**:
カーネル初期化の完了後に最初に呼ばれるユーザプログラムの入口。ここから戻るとカーネルは終了する。
_Avoid_: main、エントリポイント

**T-Monitor コンソール**:
カーネル付属のデバッグ用シリアル出力。`tm_printf` 等の出力先で、VCP に現れる。
_Avoid_: printf、UART ログ、コンソール（単独）

**システムメモリ**:
カーネルが動的割当てに使う内部 SRAM の領域。上端は例外スタック領域で区切られる。
_Avoid_: ヒープ、Imalloc 領域、arena

**例外スタック領域**:
システムメモリの上端に予約される、割込みと例外の処理に使うスタックのための領域。
_Avoid_: MSP 領域、割込みスタック、exception stack

### プロジェクト構成

**参照プロジェクト**:
TRON Forum 配布の `mtk3bsp2_stm32n657` STM32CubeIDE プロジェクト（配布 zip そのもの）。
_Avoid_: オリジナル、upstream、元プロジェクト、pristine

**parity bring-up**:
参照プロジェクトと同じソース・同じ構成のまま実機で動作を確認する段階。最小化はこの後に行う。
_Avoid_: 最小構成、minimal hello world、MVP

**VCP**:
ST-LINK が提供する仮想 COM ポート。T-Monitor コンソールの出力を PC で読む口。
_Avoid_: シリアル、UART、COM ポート

**CMake ビルド**:
ルートの `CMakeLists.txt` と `CMakePresets.json` による CLI と VS Code 用のビルド経路。CubeIDE の `.cproject` と並存し、生成イメージは parity 検査で一致を保つ。
_Avoid_: cmake ビルド、CLI ビルド、ninja ビルド

**parity 検査**:
CubeIDE ビルドと CMake ビルドの ELF を、オブジェクト集合・シンボル集合・セクションサイズで比較する検証。手順と許容差は ADR 0003。
_Avoid_: 同一性チェック、ビルド比較、diff

**ホストテスト**:
HAL と μT-Kernel に依存しない C モジュールを PC 上でビルドして実行する検査。`tests/` に独立した CMake プロジェクトとして置き、`ctest --preset host-test` で走る。
_Avoid_: ユニットテスト（単独で）、PC テスト、単体テスト

**参照ツールチェーン**:
GNU Tools for STM32 14.3.rel1。CubeIDE 2.1.1 と STM32CubeCLT 1.22.0 に同梱される。手元の CubeIDE ビルドと CMake ビルドはこれを使う。
_Avoid_: ST の gcc、CubeIDE の gcc

**CI ツールチェーン**:
Arm 公式 `arm-gnu-toolchain-14.3.rel1`。参照ツールチェーンの元になった同一の GCC 14.3.1。CubeCLT は ST のログインが必要で GitHub-hosted runner に入れられないため CI ではこちらを使う。ツールチェーンファイルは環境変数 `ARM_TOOLCHAIN_DIR` でこれを指せる。
_Avoid_: upstream gcc、公式 GCC

**CI**:
GitHub Actions でホストテストと CMake クロスビルドを実行する自動検証。**ワークフローは未追加**。導入するときはホストテストを先に入れ、ARM クロスビルドは両ターゲットのリンクが通るところまでを見る。実機と CubeIDE ビルドは CI では動かさない。CI と手元で結果が食い違ったら、まず CI 側のツールチェーン差を疑う。
_Avoid_: 自動ビルド、パイプライン、CI ゲート
