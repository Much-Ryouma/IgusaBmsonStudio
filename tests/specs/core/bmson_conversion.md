# bmson と編集モデルの変換 (core/bmson)

- 対象: `ibs::core::bmson::importDocument` / `exportDocument` / `describePlaybackDifferences`
- テストコード: `tests/unit/core/bmson/BmsonImportTest.cpp`、`BmsonExportTest.cpp`、`BmsonEquivalenceTest.cpp`
- 根拠: bmson 仕様「Sound Channels」「Slicing Algorithm」「Edge Cases」、SPEC.md §4、§4.1

## 何を保証するか

bmson の SoundChannel から、配置（c = false で再生し直す位置）・切断点・レーン割当を復元できること。
編集モデルから bmson を書き出すと、プレーヤーでの再生内容が元と同じになること。

## 読み込み (BMI)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| BMI-001 | 1チャンネル、y=0 (c=false), y=240 (c=true), y=480 (c=true) | 読み込む | 配置 1 つ（開始 0）、切断点 3 つ（offset 0, 240, 480） |
| BMI-002 | 1チャンネル、y=0 (c=false), y=960 (c=false) | 読み込む | 配置 2 つ（開始 0 と 960）。同じ素材を参照する |
| BMI-003 | y=240 に x=1 と x=7 のノーツ | 読み込む | 1つの切断点に、レーン 1 と 7 のノーツ |
| BMI-004 | y=240 に x=0 と x=3 のノーツ | 読み込む | x=0 は省略され、レーン 3 のノーツだけになる（通知 IMP-BGM-SHADOWED） |
| BMI-005 | y=240 に x=0 のノーツだけ | 読み込む | 切断点はあり、ノーツはない（BGM として鳴る） |
| BMI-006 | y=240 に c=true と c=false のノーツが混在 | 読み込む | c=false として扱い、新しい配置になる（通知 IMP-MIXED-C） |
| BMI-007 | 最初のノーツが c=true | 読み込む | そこを配置の開始として扱う（通知 IMP-FIRST-CONTINUES） |
| BMI-008 | x=12 のノーツ（beat-7k の範囲外） | 読み込む | レーン 12 のノーツとして保持する（BGM 列） |
| BMI-009 | 同じ名前のチャンネルが 2 つ | 読み込む | 素材は 1 つ、配置はチャンネルごとに別の channelGroup |
| BMI-010 | info, lines, bpm_events, stop_events, bga がある | 読み込む | 編集モデルにそのまま引き継がれる |

## 書き出し (BME)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| BME-001 | 配置の先頭切断点と、offset 240 の切断点 | 書き出す | 先頭は c=false、それ以外は c=true |
| BME-002 | ノーツのない切断点 | 書き出す | x=0 のノーツが 1 つ出力される |
| BME-003 | レーン 1 と 7 のノーツがある切断点 | 書き出す | 同じ y に x=1 と x=7、x=0 は出力されない |
| BME-004 | 同じ素材・同じ channelGroup の配置 2 つ（時間が重ならない） | 書き出す | 1つの SoundChannel に、c=false が 2 回現れる |
| BME-005 | 同じ素材・同じ channelGroup の配置 2 つ（前の配置の切断点が、後の配置の開始より後ろにある） | 書き出す | 別々の SoundChannel に分かれる |
| BME-006 | 長さ 480 のノーツ | 書き出す | l=480 |
| BME-007 | どんな編集モデルでも | 書き出す | version は "1.0.0"、ノーツは y→x の順 |

## 再生内容の同値判定 (BEQ)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| BEQ-001 | 同じ内容で、ノーツとチャンネルの並び順だけ違う 2 文書 | 比較する | 差分なし |
| BEQ-002 | 片方だけ、プレイアブルなノーツと同じ y に x=0 がある | 比較する | 差分なし（仕様上 x=0 は捨てられるため） |
| BEQ-003 | ノーツの x が 1 つ違う | 比較する | sound_channels の差分が報告される |
| BEQ-004 | info.title が違う | 比較する | info.title の差分が報告される |
| BEQ-005 | 編集モデルを書き出し、読み込み、もう一度書き出す | 1回目と2回目を比較する | 差分なし |
