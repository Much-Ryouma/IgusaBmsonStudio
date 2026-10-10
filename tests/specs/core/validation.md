# 素材パスと検証ルール (core/asset, core/validation, core/mode)

- 対象: `ibs::core::findUnsafePathReason` / `resolveSoundFile` / `validateProject` / `KeyMode`
- テストコード: `tests/unit/core/asset/AssetPathTest.cpp`、`tests/unit/core/validation/ValidatorTest.cpp`、`tests/unit/core/mode/KeyModeTest.cpp`
- 根拠: bmson 仕様「name :: DOMString」（拡張子の探索、危険なパス）、SPEC.md §10（検証ルール）、§4.1

## 何を保証するか

危険なパスを拒否し、拡張子が違っても bmson 仕様どおりに音素材を見つけられること。
保存前の検証で、プレーヤーで問題になる譜面をエラーとして止められること。

## 素材パス (AST)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| AST-001 | `C:\a.wav`、`/etc/a.wav`、`\a.wav` | 安全か調べる | すべて「絶対パス」 |
| AST-002 | `../a.wav`、`sub/../../a.wav`、`sub\..\a.wav` | 安全か調べる | すべて「親フォルダを参照」 |
| AST-003 | NUL 文字を含む名前、空の名前 | 安全か調べる | どちらも安全でない |
| AST-004 | `drums/kick.wav`、`intro\drum`、`ドラム/キック.wav` | 安全か調べる | すべて安全 |
| AST-005 | `kick.wav` と書かれ、実際は `kick.ogg` だけがある | 素材を探す | `kick.ogg` が見つかる |
| AST-006 | 拡張子なしの `piano`、`piano.m4a` だけがある | 素材を探す | `piano.m4a` が見つかる |
| AST-007 | `intro\drum.wav`、`intro/drum.wav` がある | 素材を探す | 区切り文字を `/` にそろえて見つかる |
| AST-008 | `kick.wav`、どの候補もない | 素材を探す | 見つからない |

## 検証 (VAL)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| VAL-001 | init_bpm = 0 | 検証する | エラー V01 |
| VAL-002 | resolution = 0 | 検証する | エラー V02 |
| VAL-003 | 素材名が `../a.wav` | 検証する | エラー V03 |
| VAL-004 | 素材ファイルがない | 検証する | エラー V04 |
| VAL-005 | BGA の画像がない | 検証する | 警告 V04（保存は止めない） |
| VAL-006 | レーン 1 の y=0 に長さ 480 の LN、y=480 に通常ノーツ | 検証する | エラー V05（LN の終点と次のノーツが同じ位置） |
| VAL-007 | レーン 1 の y=0 に長さ 480 の LN、y=481 に通常ノーツ | 検証する | V05 なし |
| VAL-008 | 別チャンネルの同じレーン・同じ y に、長さ 0 と 240 のノーツ | 検証する | エラー V06 |
| VAL-009 | 素材が `a.mp3` | 検証する | 警告 V07 |
| VAL-010 | beat-7k で、レーン 1 と 12 に LN が重なっている | 検証する | レーン 12（BGM 列）は V05 の対象外 |
| VAL-011 | beat-7k で、プレイアブル 2 つ（うち 1 つはレイヤーノーツ 2 個）と BGM 列 1 つ | 検証する | 情報 V11「総ノーツ数: 2（total の目安: 2）」。目安は 7.605 × N ÷ (0.01 × N + 6.5) を四捨五入 |
| VAL-012 | モード定義が見つからない | 検証する | 警告 V00 |
| VAL-013 | pulse 960 の配置を「テンポ変更の影響あり」にした | 検証する | 警告 V12（配置の位置 y=960 を含む） |

## モード定義 (KEY)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| KEY-001 | beat-7k（x=1〜8） | x=1, 8, 0, 9 がプレイアブルか調べる | 1 と 8 だけ true |
| KEY-002 | 同じ x のレーンが 2 つ | KeyMode を作る | `std::invalid_argument` |
| KEY-003 | bgm_column_start_x がレーンの x 以下 | KeyMode を作る | `std::invalid_argument` |
| KEY-004 | 同じ mode_hint を 2 回登録 | カタログに追加 | `std::invalid_argument` |
