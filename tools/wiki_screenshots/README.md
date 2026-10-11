# Wiki のスクリーンショットを作り直す

GitHub Wiki（使い方の説明）の画面写真は、合成した音源で譜面を作り、画面を表示せずに描いて保存しています。
画面を変えたときは、次の手順で作り直せます（市販の音源や手元の作品は使いません）。

```bat
:: 1. 説明用の音源（drums / bass / lead の wav と画像）を作る（Python 3 と numpy・Pillow が必要）
python tools\wiki_screenshots\make_demo_song.py

:: 2. ツールをビルドする
cmake --preset windows-msvc -DIBS_BUILD_DOC_TOOLS=ON
cmake --build --preset windows-msvc-release --target ibs_wiki_screenshots

:: 3. 画面を保存する（demo フォルダは書き換わるので、毎回作り直したものを使う）
build\msvc\tools\wiki_screenshots\Release\ibs_wiki_screenshots.exe demo modes shots

:: 4. 番号の注釈を付けて、Wiki の images フォルダへ書き出す
python tools\wiki_screenshots\annotate.py shots ..\IgusaBmsonStudio.wiki\images
```

- `make_demo_song.py` は、今いるフォルダの下に `demo/` を作ります
- 画面の各部品の位置は `shots/regions.json` に書き出され、`annotate.py` の `CALLOUTS` の順に番号が付きます。Wiki の表の番号と合わせてください
- 日本語のフォントは Noto Sans CJK JP を使います（なければ OS の既定のフォント）
