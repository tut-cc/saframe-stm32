# 0007: 33 ms期限のためST-YOLOX 320 COCO-Personを選択する

Status: accepted (2026-09-28)

## Context

ADR-0006の安全化済みフレーム公開ゲートと3段パイプラインを、480 x 480入力の
ST-YOLOX COCO-Personモデルで実機計測した。NPU推論は約28.5 ms、PSRAM上の
NNステージングから固定`nn_in`へのコピーは約6 msで、両snapshot完了から安全化
処理完了までの合計が約35 msになった。このため全フレームが33 ms期限を超えて
破棄され、起動時の黒背景から更新できなかった。

安全期限を緩和せずデモ映像を公開するには、パイプライン構造ではなくモデル推論と
入力コピーを短縮する必要がある。

## Decision

公式COCO-Personの`st_yoloxn_d033_w025_320_int8.tflite`へ変更する。入力は
320 x 320 x 3 RGB UINT8、生成後の出力はINT8、クラスは`person`だけとする。
STM32上ではクラス0を`FACE`と表示し、人物矩形全体へMASKまたはMOSAICを適用する。

モデルはSTM32 AI Model Zooのコミット
`1423c78953a830903485135febe1dd98ff31aed8`から取得し、元TFLiteのSHA-256
`e5a8a27200ba0d6ad7e0099c7bb373e605df7320647cd265d186788f1823fbfa`を検査する。
Neural-ART生成にはModel Zoo Services
`0f6210ed5156126b782e1c43249063a477484b20`、STEdgeAI Core 4.0.1、
STM32 MCU 12.0.1を使う。生成コードに合わせ、NPUランタイムも
`ll_aton` 1.1.3-dev275へ同期する。公式Getting Started v2.3.0の構成を基準にするが、
同版同梱のSTEdgeAI 4.0.0と4.0.1生成物を混在させない。STAIヘッダー、LL_ATONの
全ソース、デバイスキャッシュ実装、NetworkRuntime 12.0.1静的ライブラリを同じ
STEdgeAI 4.0.1配布物から一括更新する。

## Consequences

- NNステージングは691,200 bytes/面から307,200 bytes/面へ縮小する。
- 公式参考値はSTM32N6570-DKで推論13.29 ms、COCO-Person AP 51.17%であり、
  480版より検出精度は下がるが33 ms期限への余裕が増える。
- 4面RGB565背景、2面NNステージング、Capture/Inference/Renderの3段構成、
  RAW非公開、期限超過Drop、直前の安全化済みフレーム保持は変更しない。
- `DOCUMENT`と`LOGO`の代理名は将来の3クラスモデル用に残すが、現在モデルで
  有効な検出クラスは`FACE`だけとする。
- 検出性能が不足する場合は416 INT8、33 msを満たさない場合は256 INT8を
  次候補とする。

## Verification

- 取込前に320 x 320 x 3、UINT8入力、INT8出力、person 1クラス、40/20/10
  グリッド、モデル署名を自動検査する。さらに生成`network.c`が要求するLL_ATON
  バージョン、STAI 4.0.1、NetworkRuntime 12.0.1の一致を検査する。
- ホスト試験とAppli Debugビルド後、新しい`network_data.hex`をXSPI2へ書き込む。
- 実機では100フレームのウォームアップ後に1,000フレーム以上を測定し、公開した
  全フレームが33,000 us以内、平均および1秒区間の公開速度が15 fps以上、Fault、
  assert、カメラタイムアウト、RAW公開が0件であることを確認する。
