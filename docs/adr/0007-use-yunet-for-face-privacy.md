# 0007: 顔プライバシー検出にYuNet 320を使用する

Status: accepted (2026-09-25)

ADR-0006で導入した安全化済みフレーム公開ゲートは維持し、検出モデルを
ST-YOLOX 480からYuNet 320 x 320 int8へ変更する。利用目的が配信カメラにおける
顔のMASK/MOSAICであるため、汎用物体検出よりも顔に特化したモデルを優先する。

## Context

ST-YOLOX 480は推論が約30 ms、RGB565 MOSAICと合わせると33 ms期限を超える
フレームが多い。また、目的は任意物体の検出ではなく顔の非公開化である。
`develop/add-yunet-object-detection`にはSTM32 AI Model ZooのYuNet生成物と後処理が
あるため、それをフェイルセーフ公開ゲートへ統合する。ただし、そのブランチは
実機で顔検出できた正常系ではなく、未動作の状態だった。生成物の入力は
`STAI_FLAG_CHANNEL_FIRST` / `{1,3,320,320}`である一方、DCMIPP Pipe 2は
RGBRGB...のchannel-lastデータを直接書くため、入力レイアウトが一致していなかった。

YuNetのNeural-ARTコードはST Edge AI 4.0.1で生成され、LL_ATON 1.1.3 dev275を
要求する。従来のdev262とNetworkRuntime1200の組み合わせは互換でないため、
ベンダーランタイム全体を同じST Edge AI 4.0付属のdev275 / NetworkRuntime1201へ
揃える。

## Consequences

- 検出結果は`fd_pp_out_t`の顔ボックスと5キーポイントとする。プライバシー
  フィルタは顔ボックスに対して従来どおりRGB565上で処理する。
- RAWフレームをLTDCへ設定しない、33,000 us以内のみ公開する、期限超過時は
  直前の安全フレームを保持するというADR-0006の制約は変更しない。
- 旧ST-YOLOX重みはYuNet実行コードと互換性がない。ファーム書き込みとは別に、
  `Appli/FaceDetection/Model/network_data.hex`をXSPI2へ書き込む必要がある。
- 33 ms達成は実機計測で判定する。未達でも安全性を緩和せず、公開Dropとして
  観測する。
- 横長カメラ画像を正方形へ変形するFITは使わず、表示PipeとNN Pipeで同じ中央
  正方形をCROPする。左右の画角を失う代わりに顔の縦横比とROI座標系を保つ。
- YuNetの最大合成スコアとconfidence 0.20通過候補数を完了済みフレーム結果へ
  保存し、顔未検出時に入力・閾値・後処理のどこを調べるべきか判断可能にする。
- 最大スコアがほぼ0の場合に備え、NN入力RGB統計・変化ハッシュと量子化済み
  class/objectness出力範囲も同じ完了済みフレームへ保存する。Neural-ART出力は
  CPUで読む前にD-cacheを無効化する。
- 起動時のテンソルメタデータ検証に失敗した場合は推論を開始せず、黒背景を
  維持する。
- YuNet生成は公式Model Zoo Servicesと同じ`--inputs-ch-position chlast`を必須とし、
  `STAI_FLAG_CHANNEL_LAST`、NHWC `{1,320,320,3}`、12 INT8出力を自動検査する。
  STEdgeAI 4.0.1の`network.c`内にあるLL_ATON内部descriptorの`CHPos_First`は
  コンパイラ生成の内部表現であり、手修正しない。
- 正常基準は、隔離した公式Getting Startedアプリへ公式ONNXを配置し、
  STEdgeAI 4.0.1、LL_ATON 1.1.3 dev275、NetworkRuntime1201で生成・ビルドした
  構成とする。移植元ブランチを正常基準にはしない。
- プライバシー用途では誤検出より露出防止を優先し、後処理は0.20まで候補を
  抽出する。新規トラックはconfidence 0.35以上、既存トラックは0.20以上かつ
  IoU 0.15以上で更新し、旧位置40%・新位置60%で平滑化する。
- 保護ROIは左右30%・上下40%拡張する。検出を失ったトラックは1,000 ms保持し、
  250 msごとに余白を10ポイント追加して最大左右60%・上下70%まで広げる。
- 今回は全画面MASK/MOSAICへのフォールバックを導入しない。1,000 msを超える
  連続検出漏れではROIを解除するため、完全なプライバシー保証ではない。

## Verification

- CubeIDE Debug相当のクリーンビルドでYuNet後処理、dev275 LL_ATON、
  NetworkRuntime1201を含むELFがリンクできることを確認する。
- ホスト試験で顔ROIのMASK/MOSAIC、境界クリップ、複数ROI、期限判定、量子化
  スコア集計、0.35/0.20閾値、IoU対応付け、1,000 ms保持を確認する。
- 実機でYuNet重みの署名検査、顔検出、MASK/MOSAIC、公開数とDrop数を確認する。
- 公式単体アプリで正面顔のconfidenceが0.5を超えることを先に確認する。公式単体も
  検出しない場合はμT版の修正を止め、カメラ、ボード、重み、ツール版を調査する。
