# サイドカーと BGA の編集 (core/sidecar, core/model)

- 対象: `captureSidecar` / `applySidecar` / `addBgaHeader` / `removeBgaHeader` / `placeBgaEvent` / `removeBgaEvent` / `moveBgaEvent`
- テストコード: `tests/unit/core/sidecar/SidecarTest.cpp`
- 根拠: SPEC.md §6.1（サイドカー）、§6.5（BGA / BGI）

## 何を保証するか

bmson に書けない編集情報（切断点が自動か手動か、テンポ変更の警告、配置ごとの音切りの設定）を、bmson を開き直した後に元の配置・切断点へ戻せること。
bmson が他のツールで書き換えられて対応が取れない情報は、黙って捨てる（間違った配置に付けない）こと。
BGA の画像とイベントを、壊れた参照（存在しない画像 id）を作らずに編集できること。

## サイドカー (SDC)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| SDC-001 | bass を 0 と 1920 に配置。0 の配置に自動（240）と手動（480）の切断点、1920 の配置にテンポ変更の印、0 の配置に 8 分グリッドの音切り設定 | 保存用に取り出し、bmson から読み直した状態（切断点はすべて「読み込み」）に戻す | 2 配置とも対応が取れ、切断点 2 つの自動/手動が戻る（先頭は読み込みのまま）。1920 の配置に印、0 の配置に 8 分の設定 |
| SDC-002 | SDC-001 の情報 | 配置を 960 に動かした bmson に戻す | 対応の取れない配置 2、切断点は「読み込み」のまま |
| SDC-003 | SDC-001 の譜面 | 保存用に取り出す | 「読み込み」の切断点（各配置の先頭）は書かない。0 の配置は 240（自動）と 480（手動）の 2 つだけ |

## BGA の編集 (BGE)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| BGE-001 | id 5 の intro.png がある | main.png を追加 / intro.png を追加 / 空の名前 | 6（最大 + 1）/ 既存の 5 を返し、増えない / `std::invalid_argument` |
| BGE-002 | a.png と b.png | BGA に 960:a → 0:a → 960:b、POOR に 960:a / LAYER に存在しない id / 0 を 960 へ動かす / 960 を 1920 へ / 0 を 2 回消す | BGA は 0:a、960:b（同じ位置は置き換え、y 順）、POOR は別に 1 つ / `std::invalid_argument` / 埋まっているので `std::invalid_argument` / 1920:b になる / 2 回目は `std::invalid_argument` |
| BGE-003 | BGA に a、LAYER に b のイベント | a を削除 → もう一度削除 | a の画像とイベントが消え、b は残る → `std::invalid_argument` |
| BGE-004 | 譜面のフォルダに movie.webm と img/bg.png がある。BGA に movie.webm・img/bg.png・../other/intro.mp4・gone.png | 譜面のフォルダから届かない BGA を探す | ../other/intro.mp4（フォルダの外）と gone.png（ない）。サブフォルダの img/bg.png は届く |
