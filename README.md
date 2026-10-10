# IgusaBmsonStudio

パラデータから [bmson](https://bmson-spec.readthedocs.io/en/master/doc/index.html) 譜面を作るための制作ツールです。

- **音切り**: 音の立ち上がりを自動で検出して切断点を置く（感度調整、クオンタイズ、手動修正、スライス単位の試聴）
- **譜面編集**: 切断点の時間を固定したまま、レーン割当だけを操作できるエディタ
- **プレビュー**: 外部プレーヤー（[Pulsus](https://github.com/GoaLitiuM/Pulsus)）で任意の小節から再生
- **出力**: bmson、MIDI（切断点）

> 開発中です（マイルストーン M0: プロジェクト骨格）。現時点ではまだ譜面を作れません。

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

GoogleTest と CLI11 は、CMake の構成時に自動でダウンロードされます。

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

## ディレクトリ構成

```
src/core     ドメインモデルと計算ロジック（標準 C++ のみ）
src/infra    ファイル形式・音声・外部プロセスとの接続
src/app      ユースケース、Undo/Redo
src/gui      Qt の画面
src/cli      コマンドラインツール
tests/specs  テスト仕様（markdown）
tests/unit   テストコード（GoogleTest）
modes        譜面モードの定義（レーン構成）
midi_profiles MIDI 出力の形式定義
```

## ライセンス

[MIT](LICENSE)

依存ライブラリ: Qt（LGPL v3、動的リンク）、CLI11（BSD-3-Clause）、GoogleTest（BSD-3-Clause、テストのみ）
