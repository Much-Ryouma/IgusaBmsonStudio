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
 └ gui/       手動テストのチェックリスト
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
| [core/timing.md](core/timing.md) | pulse ⇔ 秒の変換（`TimingMap`） | TIM | 実装済み |
| [core/project.md](core/project.md) | 編集モデル（素材・配置・切断点・ノーツ） | PRJ | 実装済み |
| [core/bmson_conversion.md](core/bmson_conversion.md) | bmson ⇔ 編集モデルの変換、再生内容の同値判定 | BMI / BME / BEQ | 実装済み |
| [core/validation.md](core/validation.md) | 素材パス、検証ルール、モード定義 | AST / VAL / KEY | 実装済み |
| [infra/bmson_json.md](infra/bmson_json.md) | JSON の読み書き、ファイル操作、モード定義ファイル | JSN / FIO / KMJ | 実装済み |
| [app/chart_service.md](app/chart_service.md) | 開く・検証・保存、完成品での往復テスト | APP / EXT | 実装済み |

## 手元の作品で往復テストを実行する

実際の作品はリポジトリに入れられないため、`EXT` のテストは環境変数でフォルダを指定したときだけ実行されます。

```bat
set IBS_EXTERNAL_SAMPLES_DIR=D:\path\to\charts
ctest --preset windows-msvc-release -R SampleRoundTrip --output-on-failure
```
