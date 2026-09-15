# 0005: VSCode 側のツールパスは CubeCLT 1.22.0 の絶対パスに固定し、エディタ拡張は ST 拡張パックのみ

Status: accepted (2026-09-11)

`.vscode/settings.json` に `cmake.cmakePath` と `cmake.environment.PATH`（CubeCLT の GCC と Ninja）を固定し、toolchain ファイルは `file(GLOB)` で CubeCLT のディレクトリを展開してから `find_program` の HINTS に渡す。デバッグは ST 拡張の `stlinkgdbtarget` で Appli、FSBL の順に 2 イメージをロードする。

## Context

Codex（gpt-5.6-sol）のレビューで判明した点:

- Dock から起動した VSCode / CMake Tools はシェルの PATH を継承しない。GCC だけ探しても cmake、ninja、`arm-none-eabi-size` のどれかで止まる。POST_BUILD は `${CMAKE_SIZE}` を使う。
- `find_program` の HINTS はワイルドカードを展開しない。
- `deviceName` は CubeMX の部品名 `STM32N657X0HxQ` ではなく CMSIS の変種名 `STM32N657X0H3Q`。拡張はデバイス名を完全一致で検索し、見つからなければ接続前にエラーになる。
- `program` と `imagesAndSymbols` のパスは拡張が `existsSync` で事前検証するため `${workspaceFolder}` 付きの絶対パスにする。
- ST 拡張パックは clangd と CMake Tools を含む。cpptools と `c_cpp_properties.json` を足すと二重インデクサになる。

## Consequences

- CubeCLT の版を更新したら `.vscode/settings.json` と `.clangd` の `--query-driver` も更新する。
- 2 イメージのロードは 2026-09-15 に顔検知アプリで実機確認した。VSCode の F5 で
  `build/Debug` の Appli、FSBL の順にロードし、`usermain` から実行して顔検出まで動作した。
  CubeIDE の launch を第一とする方針は変えないが、VSCode 経路も実機で使える。
