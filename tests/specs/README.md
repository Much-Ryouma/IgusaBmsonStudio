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

| ファイル | 対象 | 状態 |
|---|---|---|
| [core/timing.md](core/timing.md) | pulse ⇔ 秒の変換（`TimingMap`） | 実装済み |
