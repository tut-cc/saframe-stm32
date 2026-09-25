# 0007: 顔プライバシー検出にYuNet 320を使用する

Status: accepted (2026-09-25)

ADR-0006で導入した安全化済みフレーム公開ゲートは維持し、検出モデルを
ST-YOLOX 480からYuNet 320 x 320 int8へ変更する。利用目的が配信カメラにおける
顔のMASK/MOSAICであるため、汎用物体検出よりも顔に特化したモデルを優先する。

## Context

ST-YOLOX 480は推論が約30 ms、RGB565 MOSAICと合わせると33 ms期限を超える
フレームが多い。また、目的は任意物体の検出ではなく顔の非公開化である。
`develop/add-yunet-object-detection`にはSTM32 AI Model ZooのYuNet生成物と後処理が
あるため、それをフェイルセーフ公開ゲートへ統合する。

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

## Verification

- CubeIDE Debug相当のクリーンビルドでYuNet後処理、dev275 LL_ATON、
  NetworkRuntime1201を含むELFがリンクできることを確認する。
- ホスト試験で顔ROIのMASK/MOSAIC、境界クリップ、複数ROI、期限判定を
  継続して確認する。
- 実機でYuNet重みの署名検査、顔検出、MASK/MOSAIC、公開数とDrop数を確認する。
