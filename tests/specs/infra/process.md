# 外部プロセスの実行 (infra/process)

- 対象: `quoteWindowsArgument` / `runProcessAndWait`
- テストコード: `tests/unit/infra/process/ProcessRunnerTest.cpp`
- 根拠: SPEC.md §11.4（プレーヤーで書き出してのヌルテスト）、§6.7（起動コマンドの引数）

## 何を保証するか

プレーヤーに渡す引数（空白や日本語を含む曲のフォルダなど）が、1 つの引数のまま届くこと。
プレーヤーの終了を待ち、終了コードを返すこと。起動できないときは例外にすること。

## テスト項目

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| PRC-001 | `plain` / 空 / `C:\My Songs\a.bmson` / `C:\My Songs\`（末尾が \） / `say "hi"` | Windows の規則で引用する | そのまま / `""` / `"C:\My Songs\a.bmson"` / `"C:\My Songs\\"`（閉じる引用符の前の \ は 2 倍）/ `"say \"hi\""` |
| PRC-002 | Windows 以外 | `sh -c "exit 3"` / 作業フォルダ `/` で `pwd` を確かめる / 存在しないプログラム | 3 / 0 / `ProcessError` |
