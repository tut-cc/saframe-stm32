# Saframe

Saframe は、カメラに映った人物を隠し終えてから映像を外へ出す、プライバシー保護カメラです。
μT-Kernel 3.0 を搭載した STM32N6570-DK 上で動き、人物の検出から映像の加工まで、すべてボードの中で行います。

TRON プログラミングコンテスト 2026、RTOS アプリケーション部門の学生部門への応募作品です。

## リリース

ビルド済みイメージは [GitHub Releases](https://github.com/tut-cc/saframe-stm32/releases) から
`saframe-v1.0.zip` をダウンロードしてください。ソースコードは同じバージョンの
[`v1.0` タグ](https://github.com/tut-cc/saframe-stm32/tree/v1.0) に固定されています。

ZIPには次のファイルが入っています。

| ファイル | 用途 |
| --- | --- |
| `mtk3bsp2_stm32n657_Appli.elf` / `.bin` | μT-KernelとSaframe本体を含むAppli |
| `mtk3bsp2_stm32n657_FSBL.elf` / `.bin` | dev-boot用のFSBL |
| `network_data.hex` | STM32CubeProgrammerで外部NORへ書き込むモデル重み |
| `network_data.xSPI2.bin` | モデル重みの生バイナリ |

### v1.0を書き込んで起動する

このリリースはDevelopment modeで使うdev-boot専用です。flash-boot用の署名済み
イメージではありません。flash-bootで起動したい場合は、ソースからビルドして
「Flash boot で起動する」の手順を使ってください。

1. ボードをDevelopment modeにして、ST-LINK USBをパソコンへ接続します。
2. ZIPを展開し、モデル重みを外部NORへ書き込みます。`STM32N6_LOADER`は
   STM32CubeProgrammerに付属する外部ローダーの実際のパスへ置き換えてください。

   ```bash
   export STM32N6_LOADER="<STM32CubeProgrammer>/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr"
   STM32_Programmer_CLI -c port=SWD freq=1000 mode=UR reset=HWrst -el "$STM32N6_LOADER" \
     -w network_data.hex -hardRst
   ```

   SWDを高速にしたり`mode=HOTPLUG`で接続したりすると、書き込み中にST-LINKのUSB通信が
   止まる環境がありました。書き込みが終わったら、ST-LINKのUSBケーブルを抜き差しして
   ボードの電源を入れ直してください。入れ直さずに起動すると外部NORを初期化できず、
   モデル重みを読む時点でBusFaultになります。

3. `v1.0`タグのソースをSTM32CubeIDEへインポートし、展開したAppli/FSBLの
   `.elf`をそれぞれ`Appli/Debug/`と`FSBL/Debug/`へ置きます。
4. `Appli/mtk3bsp2_stm32n657_Appli Debug.launch`を開始します。このlaunch構成が
   FSBLとAppliを内部SRAMへロードしてFSBLから起動します。
5. `usermain`で停止したら実行を再開します。T-Monitorコンソールは
   115200 bps、8-N-1です。

ソースから再ビルドする場合は、次の「ビルドと実行」の手順を使ってください。

## 特長

- **ボード内で完結**
  人物の検出から映像の加工まで、STM32N6570-DK の中だけで行います。映像をクラウドへ送る必要はありません。
- **間に合わないコマは捨てる**
  取り込みから隠し終えるまでに、30 fps の動画 1 コマ分にあたる 33 ミリ秒の制限時間を設けています。間に合わなかったコマは表示せずに捨て、直前の安全なコマを表示し続けます。
- **画面と USB Web カメラへ同時に出力**
  ボードの液晶画面に表示しながら、USB でつないだパソコンへ H.264 の Web カメラ映像として送ります。

## 仕組み

```mermaid
flowchart LR
  subgraph private["外から見えない領域"]
    cam["カメラ IMX335"] --> buf["作業バッファ"]
    buf --> nn["物体検出<br/>ST-YOLOX を NPU で推論"]
    nn --> filt["黒塗り / モザイク"]
  end
  filt --> gate{"処理期限<br/>33 ms 以内か"}
  gate -- "間に合った" --> lcd["液晶画面 800×480"]
  gate -- "間に合った" --> usb["USB Web カメラ<br/>H.264 320×240 30 fps"]
  gate -- "超過" --> drop["破棄して<br/>直前のコマを表示"]
```

カメラは表示用と推論用の 2 系統の画像を 30 fps で連続して取り込みます。同じ瞬間に撮られた 2 枚だけを組にし、物体検出モデルで人物を探して、その領域を隠します。各バッファは「空き、取り込み中、取り込み済み、推論中、加工済み、表示または破棄」の順にしか状態を進めません。これにより、加工前のコマが表示されることを構造として防いでいます。

### μT-Kernel のタスク

| タスク | 優先度 | 役割 |
| --- | ---: | --- |
| 描画 | 3 | 黒塗り・モザイク、制限時間の判定、液晶画面の切り替え。USB 用の縮小コピーの間だけ優先度を 6 に下げる |
| 取り込み | 4 | カメラの 2 系統の画像を組にして後段へ渡す |
| 推論 | 5 | 物体検出モデルを NPU で実行し、人物の位置を求める |
| H.264 圧縮 | 7 | USB へ送る映像を VENC で圧縮する |
| USB | 8 | USB の通信処理。割込みからセマフォで起こされる |
| 監視・操作 | 12 | ボタンでの隠し方の切り替えと、1 秒ごとの計測値の出力 |

優先度は数値が小さいほど高くなります。バッファの受け渡しにはセマフォを、処理結果の共有には優先度継承つきのミューテックスを使っています。

## 性能

STM32N6570-DK の実機で、T-Monitor に 1 秒ごとに出る計測値を集計した結果です。黒塗りとモザイク、USB 出力の有無を切り替えながら、映る人数も変えて約 20 分連続で動かしました。

- 約 36,000 コマを処理し、障害やカメラの停止は起きませんでした。
- 公開したコマの処理時間は最大 32,999 マイクロ秒で、すべて 33 ミリ秒の制限時間に収まっています。
- 物体検出モデルの推論時間は、どの条件でも約 13.2 ミリ秒です。

| 条件 | 公開 fps の平均 | 制限時間を超えて捨てた割合 |
| --- | ---: | ---: |
| 黒塗り | 30.3 | 0.07% |
| 黒塗りと USB 出力 | 29.8 | 1.2% |
| モザイク | 27.4 | 7.9% |
| モザイクと USB 出力 | 17.9 | 32% |

モザイクの処理時間は隠す面積に比例して延びます。映る人が 1 人なら捨てたコマは約 2% ですが、3 人になると半数を超えます。間に合わないコマは表示されず、画面は直前の安全なコマのまま止まります。加工前の映像は出ません。モザイク処理の高速化は今後の課題です。

## 必要なもの

- STM32N6570-DK と、付属の IMX335 カメラモジュール
- STM32CubeIDE と STM32CubeProgrammer
- USB Type-C ケーブル 2 本。デバッグ用の ST-LINK と、Web カメラ出力用の CN18 に使う
- USB 映像を見る場合は、H.264 を再生できるアプリ。Windows では FFmpeg の `ffplay`、Linux では VLC や guvcview

動作を確認した開発環境は、STM32CubeIDE 2.1.1、GNU Tools for STM32 14.3.rel1、STM32CubeProgrammer 2.22.0 です。

## ビルドと実行

1. STM32CubeIDE でこのリポジトリのルートを開き、`mtk3bsp2_stm32n657`、`mtk3bsp2_stm32n657_Appli`、`mtk3bsp2_stm32n657_FSBL` の 3 つのプロジェクトをインポートします。
2. Appli と FSBL を `Debug` 構成でビルドします。
3. ボードを Development mode にし、モデルの重みを外部フラッシュへ一度だけ書き込みます。

   ```bash
   export STM32N6_LOADER="<STM32CubeProgrammer>/bin/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr"
   STM32_Programmer_CLI -c port=SWD freq=1000 mode=UR reset=HWrst -el "$STM32N6_LOADER" \
     -w Appli/FaceDetection/Model/network_data.hex -hardRst
   ```

   書き込みが終わったら、ST-LINK の USB ケーブルを抜き差ししてボードの電源を入れ直します。理由は上の「v1.0を書き込んで起動する」を参照してください。

4. `Appli/mtk3bsp2_stm32n657_Appli Debug.launch` でデバッグを開始し、`usermain` で止まったら実行を再開します。
5. ST-LINK の仮想 COM ポートを 115200 bps、8-N-1 で開くと、T-Monitor に計測値が 1 秒ごとに出力されます。

### 使い方

- 人物がカメラに映ると、液晶画面では隠された状態で表示されます。
- 青いユーザーボタン B2 を押すと、黒塗りとモザイクが切り替わります。
- CN18 とパソコンをつなぐと、パソコンからは Web カメラとして見えます。Windows では次のように再生します。デバイス名は `ffmpeg -list_devices true -f dshow -i dummy` で確認してください。

  ```bash
  ffplay -f dshow -i video="STM32 uvc"
  ```

  映像は H.264 のみです。MJPEG にしか対応しないアプリ、たとえば Windows 標準のカメラアプリでは表示できません。

## Flash boot で起動する

上の手順は、デバッガがプログラムを内部 SRAM へ直接ロードする dev-boot です。
ここでは、署名済みのイメージを外部 NOR フラッシュへ書き込み、デバッガなしで電源を入れるだけで起動する flash-boot の手順を説明します。

起動は次の順に進みます。

1. マイコン内蔵の Boot ROM が、外部 NOR の先頭 `0x70000000` から FSBL を読み込んで実行します。FSBL はボード起動時に最初に動く小さなブートローダです。
2. FSBL が、外部 NOR の `0x70100000` に置いた Appli を内部 SRAM へコピーして実行します。Appli は μT-Kernel と Saframe 本体を含むイメージです。
3. Appli が、外部 NOR の `0x70380000` 以降にあるモデルの重みを読みながら推論します。

Boot ROM は、先頭に署名ヘッダが付いたイメージしか実行しません。
ここで使う署名ヘッダは鍵を使わない開発用のもので、STM32CubeProgrammer に付属する `STM32_SigningTool_CLI` で付けます。

### ビルド

STM32CubeIDE で次の 2 つをビルドします。

- `mtk3bsp2_stm32n657_Appli` を `Debug` 構成でビルドします。dev-boot と同じイメージです。
- `mtk3bsp2_stm32n657_FSBL` を `Release` 構成でビルドします。

FSBL だけ構成を変えるのは、`Debug` 構成の FSBL は Appli がデバッガで SRAM へロード済みだとみなし、外部 NOR からのコピーを省くからです。
`Release` 構成の FSBL だけが、外部 NOR から Appli をコピーします。

### 署名と書き込み

ボードを Development mode にして、ST-LINK の USB ケーブルをパソコンへつなぎます。
Development mode は BOOT0 を LOW、BOOT1 を HIGH にした状態です。
そのうえで、Git Bash などの bash からリポジトリのルートで次を実行します。

```bash
tools/make_flash_images.sh --flash --with-weights
```

このスクリプトは、2 つのイメージに署名して `build/flash/` へまとめ、外部 NOR へ書き込みます。
モデルの重みを一度書き込んだあとは、`--with-weights` を外すと FSBL と Appli だけを書き換えます。
`--flash` も外すと、署名済みイメージを作るだけで書き込みません。
STM32CubeProgrammer を既定と異なる場所へインストールした場合は、`STM32_PROGRAMMER_BIN` にその `bin` ディレクトリを指定してください。

STM32CubeProgrammer の画面から書き込む場合は、External loaders で `MX66UW1G45G_STM32N6570-DK` を選び、Erasing & Programming で次の 3 つのファイルを 1 つずつ書き込みます。

| ファイル | 書き込み先アドレス |
| --- | --- |
| `build/flash/FSBL-trusted.bin` | `0x70000000` |
| `build/flash/Appli-trusted.bin` | `0x70100000` |
| `build/flash/network_data.hex` | 指定不要。ファイル内にアドレスが含まれている |

### 起動

1. 書き込みが終わったら、BOOT0 と BOOT1 をどちらも LOW にします。これが flash-boot の設定です。
2. ST-LINK の USB ケーブルを抜き差しして、ボードの電源を入れ直します。
3. 液晶画面にカメラの映像が出れば起動しています。T-Monitor の計測値も dev-boot と同じく 115200 bps、8-N-1 で出力されます。

dev-boot に戻すときは、BOOT1 を HIGH に戻してから従来どおりデバッグを開始してください。

## ディレクトリ構成

| パス | 内容 |
| --- | --- |
| `Appli/` | μT-Kernel とアプリケーション本体のイメージ |
| `Appli/FaceDetection/` | カメラ、推論、公開ゲート、USB 出力のソースとモデル |
| `FSBL/` | ボード起動時に最初に動くブートローダ |
| `modelzoo/` | STM32 AI Model Zoo で現行モデルを生成するための設定と、モデルの検査条件 |
| `tools/` | モデルの取り込みと検査、推論ランタイムの同期、学習用のスクリプト |
| `tests/` | パソコン上で動く試験 |
| `docs/adr/` | 設計判断の記録 |
| `hello_world/` | μT-Kernel が STM32N6570-DK で起動することを確かめた最小構成 |

## ドキュメント

- [README_OBJECT_DETECTION.md](README_OBJECT_DETECTION.md): 詳しい構成、モデルの差し替え手順、T-Monitor の計測項目
- [docs/adr/](docs/adr/): 設計判断の記録。特に 0006 公開ゲート、0007 モデルの選定、0008 USB 出力
- [CONTEXT.md](CONTEXT.md): このリポジトリで使う用語

## ライセンス

Saframe のために書いたコードと文書は [MIT License](LICENSE) で公開しています。
リポジトリに取り込んだ既存ソフトウェアには、それぞれのライセンスが適用されます。

## 使用している既存ソフトウェア

μT-Kernel 3.0 と BSP2 は TRON Forum が T-License 2.2 で配布しているものを使っています。
このほか、STMicroelectronics の STM32CubeN6 HAL、STM32N6 のサンプル、STM32 AI Model Zoo の学習済みモデル、UVC ライブラリ、H.264 エンコーダを取り込んでいます。
入手先、固定したバージョン、著作権者、ライセンスは [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) に記載しています。
