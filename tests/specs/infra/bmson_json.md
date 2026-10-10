# bmson の JSON 入出力 (infra/bmson, infra/fs, infra/mode)

- 対象: `ibs::infra::parseBmson` / `serializeBmson` / `loadBmsonFile` / `saveBmsonFile` / `LocalFileSystem` / `parseKeyMode`
- テストコード: `tests/unit/infra/bmson/BmsonJsonCodecTest.cpp`、`tests/unit/infra/fs/FileIoTest.cpp`、`tests/unit/infra/mode/KeyModeJsonLoaderTest.cpp`
- 根拠: bmson 仕様「Top Level Object」「Information Object」「Sound Channels」、SPEC.md §4.1、§6.1

## 何を保証するか

bmson 仕様の既定値・異常値の扱いどおりに JSON を読み込めること。
書き出した JSON を読み込み直すと同じ内容になり、既存ツールのファイルと差分を比べやすい形で書かれること。
日本語を含むパスでも、Windows で読み書きできること。

## JSON の読み書き (JSN)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| JSN-001 | version, info.title, info.init_bpm, sound_channels だけの JSON | 読み込む | mode_hint="beat-7k", judge_rank=100, total=100, resolution=240, lines なし |
| JSN-002 | 先頭に UTF-8 の BOM がある | 読み込む | 読み込める |
| JSN-003 | version がない | 読み込む | `BmsonFormatError`（1.0.0 より前の形式は非対応） |
| JSN-004 | version が "0.21" | 読み込む | `BmsonFormatError` |
| JSN-005 | info.init_bpm がない | 読み込む | `BmsonFormatError`（仕様で必須） |
| JSN-006 | resolution が 0 / null / -96 | 読み込む | 240 / 240 / 96（負の場合は通知 JSN-NEGATIVE-RESOLUTION） |
| JSN-007 | ノーツの x が null、l と c が省略 | 読み込む | x=0, l=0, c=false |
| JSN-008 | lines が空配列 | 読み込む | 「小節線なし」として空の配列を保持する（省略時の「4/4 を仮定」と区別する） |
| JSN-009 | 仕様にないフィールドがある | 読み込む | 通知 JSN-UNKNOWN-FIELD（保存すると失われることを知らせる） |
| JSN-010 | y が 240.5 / 240.0 | 読み込む | 240.5 は `BmsonFormatError`、240.0 は 240 |
| JSN-011 | info・タイミング・ノーツ・BGA を含む文書 | 書き出して読み込む | 元の文書と同じ |
| JSN-012 | init_bpm = 100.0 | 書き出す | `"init_bpm": 100`（`.0` を付けない） |
| JSN-013 | ノーツがある | 書き出す | ノーツ1つが1行に `{"x": 1, "y": 240, "l": 0, "c": true}` の形で書かれる |
| JSN-014 | 日本語のタイトル | 書き出す | `\u` でエスケープせず UTF-8 のまま書かれる |

## ファイル操作 (FIO)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| FIO-001 | 日本語のフォルダ名・ファイル名 | bmson を保存して読み込む | 同じ内容 |
| FIO-002 | 同じ名前のファイルがある | 保存する | 上書きされ、一時ファイル（.tmp）が残らない |
| FIO-003 | 日本語名の音素材ファイルとフォルダ | LocalFileSystem で存在を調べる | ファイルは true、フォルダは false |
| FIO-004 | 存在しないファイル | 読み込む | `std::runtime_error` |

## モード定義ファイル (KMJ)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| KMJ-001 | リポジトリの `modes/beat-7k.json` | 読み込む | mode_hint="beat-7k"、レーン 8 本、BGM 列は x=9 から、x=8 はスクラッチ |
| KMJ-002 | mode_hint がない | 読み込む | `std::invalid_argument` |
| KMJ-003 | kind が未知の値 | 読み込む | `std::invalid_argument` |
| KMJ-004 | リポジトリの `modes/` フォルダ | カタログとして読み込む | beat-7k が見つかる |
