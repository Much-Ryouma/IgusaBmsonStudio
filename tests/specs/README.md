# テスト仕様

このフォルダには、テストが「何を検証しているか」を人が読める形で書いた markdown を置く。
テストコード（`tests/unit/`）は、ここに書いた各項目の ID と1対1で対応させる。

## フォルダ構成

ソースのレイヤーに合わせる。

```
tests/specs/
 ├ core/      ドメインモデル・計算ロジック
 ├ infra/     ファイル入出力・音声・外部連携
 ├ app/       ユースケース・Undo
 └ gui/       画面の自動テスト（offscreen）と手動テストのチェックリスト
```

## 書き方

1ファイル＝1つの機能。[TEMPLATE.md](TEMPLATE.md) をコピーして使う。

- **ID**: `<機能の略称3〜4文字>-<3桁の連番>`（例: `TIM-001`）。一度振った ID は変えない。項目を削除したら番号は欠番にする
- **前提**: テストを始める前の状態・入力
- **操作**: テストで行うこと
- **期待結果**: 確かめること。数値は単位まで書く。計算の根拠も括弧で書く

## テストコードとの対応

| 仕様 | テストコード |
|---|---|
| `tests/specs/core/timing.md` の `TIM-003` | `tests/unit/core/timing/TimingMapTest.cpp` の `TEST(TimingMap, TIM003_NoteOnStopPulseActivatesAtStopStart)` |

- テスト名は `<ID からハイフンを除いたもの>_<何を保証するか>`
- 仕様にない項目のテストは書かない。先に仕様へ項目を追加する

## 仕様一覧

| ファイル | 対象 | ID | 状態 |
|---|---|---|---|
| [core/timing.md](core/timing.md) | pulse ⇔ 秒の変換（`TimingMap`）、BPM・STOP・拍子・小節線の編集 | TIM / TED | 実装済み |
| [core/project.md](core/project.md) | 編集モデル（素材・配置・切断点・ノーツ） | PRJ | 実装済み |
| [core/bmson_conversion.md](core/bmson_conversion.md) | bmson ⇔ 編集モデルの変換、再生内容の同値判定 | BMI / BME / BEQ | 実装済み |
| [core/validation.md](core/validation.md) | 素材パス、検証ルール、モード定義 | AST / VAL / KEY | 実装済み |
| [infra/bmson_json.md](infra/bmson_json.md) | JSON の読み書き、ファイル操作、モード定義ファイル、サイドカーファイル、画像の大きさ | JSN / FIO / KMJ / SCJ / IMG | 実装済み |
| [app/chart_service.md](app/chart_service.md) | 開く・検証・保存、完成品での往復テスト | APP / EXT | 実装済み |
| [app/editing.md](app/editing.md) | Undo、配置と切断点の操作、素材の取り込み、表示範囲とスナップ | EDT / ARR / IMP / VPT | 実装済み |
| [app/chart_commands.md](app/chart_commands.md) | 譜面編集の操作と Undo、テンポ変更後の切断点 | CMD | 実装済み |
| [app/preview_bga.md](app/preview_bga.md) | 外部プレーヤーでのプレビュー、BGA と曲の情報の操作 | PRV / BGC | 実装済み |
| [app/slicing_playback.md](app/slicing_playback.md) | 素材の読み込み、試聴、音切りのプレビューと適用 | AUL / PLY / SLS | 実装済み |
| [gui/studio.md](gui/studio.md) | 画面の操作（画面を出さずに実行する自動テスト） | GUI | 実装済み |
| [gui/manual_checklist.md](gui/manual_checklist.md) | 見た目と実際の音の手動チェックリスト | — | M6 |
| [core/slicing.md](core/slicing.md) | 音声バッファ、FFT、立ち上がりの検出、波形の縮小表示、グリッドとクオンタイズ、小節線と拍子 | AUD / FFT / ONS / WAV / QNT / CUT / APL / BAR | 実装済み |
| [core/playback.md](core/playback.md) | 試聴の範囲、曲のミックス、bmson の鳴らし方と 2 つの音の比較 | RNG / NUL | 実装済み |
| [app/statistics_nulltest.md](app/statistics_nulltest.md) | 譜面の統計、ヌルテスト、最近使った譜面 | STA / NTS / RCF | 実装済み |
| [core/sidecar_bga.md](core/sidecar_bga.md) | サイドカーの取り出しと復元、BGA の編集 | SDC / BGE | 実装済み |
| [core/chart_editing.md](core/chart_editing.md) | レーン割当、パターンの貼り付け、変形、テンポ変更でずれる配置 | CHE | 実装済み |
| [core/midi.md](core/midi.md) | MIDI の組み立て、SMF の書き出し、MIDI プロファイル | MID / SMF / MPF | 実装済み |
| [infra/audio.md](infra/audio.md) | 音声の読み込み、音切りのユースケース | DEC / SLC | 実装済み |
| [infra/process.md](infra/process.md) | 外部プロセスの実行（ヌルテストの書き出し） | PRC | 実装済み |

## 手元の作品で往復テストを実行する

実際の作品はリポジトリに入れられないため、`EXT` のテストは環境変数でフォルダを指定したときだけ実行されます。

```bat
set IBS_EXTERNAL_SAMPLES_DIR=D:\path\to\charts
ctest --preset windows-msvc-release -R SampleRoundTrip --output-on-failure
```

## 音切りの精度を手元の作品で測る

作者が実際に置いた切断点と、自動検出の結果を照合するスクリプトです（Python 3 が必要です）。テストではなく、パラメータを調整するときの目安に使います。

```bat
python tools\evaluate_slicing.py build\core-only\src\cli\igusa-cli.exe "曲\譜面.bmson" "曲\ステムのフォルダ" --sensitivity 0.6
```
