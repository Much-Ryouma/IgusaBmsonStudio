IgusaBmsonStudio
================

bmson（BMS の譜面形式）の音切り・譜面編集・プレビューのためのツールです。
THE BMS OF FIGHTERS 22 への参加作品づくりのために作りました。

■ 動作環境
  Windows 10 / 11（64bit）

■ 使い方
  1. この zip をすべて展開します（日本語を含むフォルダに置いてもかまいません）
  2. IgusaBmsonStudio.exe をダブルクリックします
  3. 使い方の説明: https://github.com/Much-Ryouma/IgusaBmsonStudio/wiki
     （アプリの「ヘルプ → 使い方（Wiki）を開く」からも開けます）

  インストーラーはありません。レジストリにも何も書きません。
  設定（プレーヤーの場所、最近使った譜面など）は、exe の横の IgusaBmsonStudio.ini に保存します。
  消すときはフォルダごと削除してください。
  ※ Program Files など、書き込めないフォルダに置くと設定を保存できません。

■ 同梱物
  IgusaBmsonStudio.exe   アプリ本体
  IgusaBmsonStudio.ini   設定（初めて設定を変えたときに作られます。消すと初期状態に戻ります）
  igusa-cli.exe          コマンドラインツール（検証・統計・ヌルテスト・MIDI 書き出しなど）
  modes\                 キーモードの定義（beat-7k など）。消さないでください
  midi_profiles\         MIDI 書き出しの設定（igusa-cli 用）
  Qt6*.dll, platforms\ など   Qt のライブラリ（動作に必要です）
  msvcp140.dll など       Visual C++ ランタイム
  LICENSE.txt            IgusaBmsonStudio のライセンス（MIT License）
  THIRD_PARTY_NOTICES.txt 同梱しているソフトウェアのライセンス
  licenses\              Qt のライセンス（LGPLv3）の全文

■ コマンドラインツールの例（コマンドプロンプトで）
  igusa-cli validate 譜面.bmson
  igusa-cli stats 譜面.bmson
  igusa-cli nulltest 譜面.bmson
  igusa-cli --help で一覧が出ます。

■ うまく動かないとき
  - 「VCRUNTIME140.dll が見つからない」と出る: zip の中身をすべて同じフォルダに展開したか確認してください
  - Pulsus で再生できない: 「再生 → 設定」で Pulsus.exe の場所を指定してください
  - 不具合の報告: https://github.com/Much-Ryouma/IgusaBmsonStudio/issues

■ ライセンス
  MIT License（LICENSE.txt）。同梱の Qt などのライセンスは THIRD_PARTY_NOTICES.txt を見てください。
