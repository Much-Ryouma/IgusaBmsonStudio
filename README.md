# IgusaBmsonStudio

パラデータから [bmson](https://bmson-spec.readthedocs.io/en/master/doc/index.html) 譜面を作るための制作ツールです。

- **音切り**: 音の立ち上がりを自動で検出して切断点を置く（感度調整、クオンタイズ、手動修正、スライス単位の試聴）
- **譜面編集**: 切断点の時間を固定したまま、レーン割当だけを操作できるエディタ
- **プレビュー**: 外部プレーヤー（[Pulsus](https://github.com/GoaLitiuM/Pulsus)）で任意の小節から再生
- **出力**: bmson、MIDI（切断点）

> 開発中です（マイルストーン M6 実装済み: 配置・音切り・割り当て・BGA・曲の情報まで GUI で作り、統計とヌルテストで確かめ、Pulsus で再生できます）。実際の作品での通しの確認を進めています。

## ドキュメント

- [仕様書](docs/SPEC.md)
- [コーディング規約](docs/CODING_RULES.md)
- [テスト仕様](tests/specs/README.md)

## ビルド

### 必要なもの

- Windows 10/11
- Visual Studio 2022（「C++ によるデスクトップ開発」）
- CMake 3.24 以上
- Qt 6.5 以上（MSVC 2022 64bit 版）

GoogleTest、CLI11、nlohmann/json、miniaudio、stb_vorbis は、CMake の構成時に自動でダウンロードされます（ハッシュで内容を確認します）。

### 手順

```bat
set CMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

生成されるもの:

| ファイル | 内容 |
|---|---|
| `build/msvc/src/gui/Release/IgusaBmsonStudio.exe` | GUI アプリ |
| `build/msvc/src/cli/Release/igusa-cli.exe` | コマンドラインツール |

GUI を実行するときは、Qt の DLL を exe の横に配置します。

```bat
C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe build\msvc\src\gui\Release\IgusaBmsonStudio.exe
```

Qt を使わずに core / CLI / テストだけをビルドする場合は、`core-only` プリセット（Ninja）を使います。

画面のテスト（`StudioGui`）は、画面を出さずに（`QT_QPA_PLATFORM=offscreen`）実行されます。Qt の DLL のフォルダは ctest が自動で PATH に加えます。

### 日本語を含むフォルダでビルドする（Building in a folder with Japanese characters）

ソースやビルドのパスに日本語などの ASCII 以外の文字が含まれると、CMake・依存ライブラリの展開・コンパイラのどこかで失敗することがあります（構成時に警告が出ます）。
フォルダを移動せずに済ませるには、`subst` で ASCII だけのドライブ名を割り当て、そちらからビルドします。

```bat
subst B: "D:\ClaudeCodeのお部屋\bmsonworkstation"
```

Qt Creator では `B:\CMakeLists.txt` を開き直してください（以前のビルドフォルダは削除してから構成し直します）。
`subst` の割り当ては再起動で消えるので、毎回実行するか、スタートアップに登録してください。

## GUI の使い方

1. **ファイル → 新しい譜面** で保存先の `.bmson`、タイトル、BPM を決める（既存の譜面は **開く**）
2. **ファイル → 素材を取り込む** でフレーズの wav / ogg を選ぶ。譜面のフォルダの外にあるファイルは、確認のうえフォルダにコピーします
3. 上のアレンジャーで、素材の行の空いた所をダブルクリックして配置する。ドラッグで移動（Ctrl で複製、Shift で細かく、Alt でスナップなし）
4. 配置をダブルクリックすると、下の波形ビューで音切りできます
   - 右のパネルで感度などを動かすと、切断点の候補（点線）がすぐに変わります。**適用** で切断点になります（元に戻すで 1 回で戻せます）
   - 波形をクリックするとそのスライスだけ鳴ります。←/→ で前後のスライスを試聴（画面の外なら自動でスクロール）。下のスクロールバーで横に動かせます
   - ダブルクリックで切断点を追加、ドラッグで移動、Delete で削除、Q でグリッドに寄せる
5. **譜面** タブでレーンを割り当てる
   - 右の素材列で切断点をクリック（試聴されます）→ `1`〜`7` / `S` で割当。自動で次の切断点へ進むので、続けて打てます。`0` で割当を外して BGM に戻します
   - ノーツの横ドラッグでレーン変更、上端か Shift+ドラッグでロングノーツ
   - 範囲選択して Ctrl+C、別の場所をクリックして Ctrl+V で、割当パターンを繰り返しのフレーズに写せます
   - 右クリックで BPM 変更・STOP・拍子・小節線
   - ノーツや切断点をクリックすると、下の波形がそのスライスの所を表示します（**譜面 → クリックしたスライスを波形に表示する** で切れます）
6. **譜面** タブ右の **BGA 画像** で画像・動画を追加し、BGA / LYR / POOR 列でダブルクリックして置く
7. **ファイル → 曲の情報** でタイトル・アーティスト・レベル・total・バナーなどを設定する
8. **F7** で外部プレーヤー（Pulsus など）がカーソルの小節から再生します。初回はプレーヤーの場所を聞かれます（**再生 → 設定**）
9. **保存** で bmson に書き出します。検証エラー（素材ファイルがない、ロングノーツが重なっているなど）があるときは保存しません
   - 同じ場所に `<name>.ibs.json`（サイドカー）も書きます。切断点が自動か手動か、音切りの設定などを覚えておくためのファイルで、提出には不要です
10. **統計 → 譜面の統計**（Ctrl+Shift+I）で、譜面・BGM・合計それぞれのノーツ数、1 秒あたりの最大密度、平均密度、再生時間、音量と、最大になる位置（小節）を確認できます
11. **統計 → ヌルテスト** で、配置どおりの音と、書き出した bmson の鳴り方が同じかを確かめます。違う区間は小節番号つきで一覧になり、ダブルクリックでそこへ移ります
    - 「内蔵の再生」はこのツールの中で bmson を鳴らして比べます。「プレーヤーで書き出し」は Pulsus の `--render` で書き出した wav と比べます（書き出しコマンドは **再生 → 設定** で変えられます）

曲の再生中はアレンジャーと譜面が再生位置に合わせてめくれます（**表示 → 再生位置に合わせてスクロールする**）。最近使った譜面は **ファイル → 最近使った譜面** から開けます。

操作の一覧はアプリの **ヘルプ → 操作方法** にあります。音切りの線の色は、青=自動、橙=手動、灰=読み込み、赤=アタック欠け です。

## コマンドラインツール

```bat
igusa-cli info     "曲[ANOTHER].bmson"            :: 譜面の概要（--json で JSON 出力）
igusa-cli validate "曲[ANOTHER].bmson"            :: 検証。エラーがあれば終了コード 1
igusa-cli convert  in.bmson out.bmson --check     :: 編集モデルを通して書き出し、再生内容が同じか確認
igusa-cli slice    bass.wav --bpm 175 --grid 16     :: 音の立ち上がりを検出し、16 分グリッドに寄せた切断点を表示
igusa-cli slice    bass.wav --chart song.bmson --bmson cuts.bmson   :: 譜面のテンポ・小節線を使い、結果を bmson に書き出す
igusa-cli midi     song.bmson -o cuts.mid           :: 切断点を MIDI に（既定はプナイプナイえんそう互換）
igusa-cli midi     song.bmson -o tempo.mid --profile generic --tempo-only   :: テンポマップだけを DAW 用に
igusa-cli stats    song.bmson                       :: ノーツ数・密度・音量（譜面 / BGM / 合計）
igusa-cli nulltest song.bmson                       :: 配置どおりの音と bmson の鳴り方を比べる（差が -30 dB を超えたら終了コード 1）
igusa-cli nulltest song.bmson --player C:\Games\Pulsus\Pulsus.exe   :: Pulsus に書き出させて比べる
igusa-cli nulltest song.bmson --rendered out.wav    :: 手元で書き出した wav と比べる
```

`slice` の主なオプション: `--sensitivity`（0〜1、既定 0.5）、`--function flux|energy|hfc`、`--min-interval`（ms）、`--gate`（dBFS）、`--preroll`（ms）、`--grid`（0 で寄せない）、`--strength`、`--window`（ms）。一覧は `igusa-cli slice --help` で確認できます。

終了コード: 0 = 成功、1 = 検証エラー、2 = 引数エラー、3 = 入出力エラー

## ディレクトリ構成

```
src/core     ドメインモデルと計算ロジック（標準 C++ のみ）
src/infra    ファイル形式・音声・外部プロセスとの接続
src/app      ユースケース、Undo/Redo
src/gui      Qt の画面
src/cli      コマンドラインツール
tests/specs  テスト仕様（markdown）
tests/unit   テストコード（GoogleTest）
tests/gui    画面のテスト（画面を出さずに実行）
tools        開発用のスクリプト（音切りの精度評価など）
modes        譜面モードの定義（レーン構成）
midi_profiles MIDI 出力の形式定義
```

## ライセンス

[MIT](LICENSE)

依存ライブラリ: Qt（LGPL v3、動的リンク）、nlohmann/json（MIT）、CLI11（BSD-3-Clause）、miniaudio（Public Domain / MIT-0）、stb_vorbis（Public Domain / MIT）、GoogleTest（BSD-3-Clause、テストのみ）
