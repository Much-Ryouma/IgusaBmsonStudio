# MIDI 出力 (core/midi, infra/midi)

- 対象: `buildCutPointMidi` / `buildTempoMapMidi` / `serializeStandardMidiFile` / `parseMidiProfile`
- テストコード: `tests/unit/core/midi/MidiExporterTest.cpp`、`tests/unit/infra/midi/StandardMidiFileTest.cpp`
- 根拠: SPEC.md §6.8（MIDI 出力）、`appendix/プナイプナイえんそう/d_bongo.mid` の構造

## 何を保証するか

切断点を、受け渡し先（プナイプナイえんそう など）が読み込める形の MIDI にできること。
テンポ変化・拍子・STOP を、プロファイルの指定どおりに書き出し、表現できないものは警告すること。

## MIDI の組み立て (MID)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| MID-001 | 素材 2 つ、プナイプナイえんそう互換プロファイル | 出力 | 指揮者トラックなし。各トラックは「トラック名 → テンポ」で始まり、ノーツは次の切断点まで隙間なく続く |
| MID-002 | 汎用プロファイル | 出力 | 先頭に指揮者トラック（テンポと拍子）。ノーツのトラックにテンポはない |
| MID-003 | resolution 240、PPQ 128、切断点 y=80 | 出力 | tick 43（80×128/240=42.67 を四捨五入）。V14 の警告 |
| MID-004 | pulse 960 で BPM 150 に変化、プナイプナイえんそう互換 | 出力 | すべてのトラックに tick 0 と tick 960 のテンポがある |
| MID-005 | 小節線 0, 960, 1800 (7/8), 2760 | 汎用で出力 | 拍子は tick 0 に 4/4、tick 960 に 7/8、tick 1800 に 4/4（変わったところだけ出る） |
| MID-006 | STOP がある | 汎用 / 互換で出力 | どちらも V13。マーカーは汎用だけ |
| MID-007 | 同じ素材を 0 と 1920 に配置、配置先頭基準 | 出力 | ファイル 2 つ。2 つ目は tick 0 から始まり、テンポは配置先頭時点の値 |
| MID-008 | BPM 変化と小節線がある | テンポだけ出力 | トラック 1 つ（名前 "Tempo"）、ノーツなし |
| MID-009 | 切断点が 1 つだけ | 出力 | ノーツの長さは 1 拍（PPQ と同じ tick 数） |

## SMF の書き出しとプロファイル (SMF, MPF)

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| SMF-001 | トラック 2 つ、PPQ 96 | 書き出す | `MThd`、長さ 6、形式 1、トラック数 2、PPQ 96 |
| SMF-002 | 間隔 0, 127, 128, 16383, 16384 tick のイベント | 書き出す | 可変長の差分が `00` `7F` `81 00` `FF 7F` `81 80 00` |
| SMF-003 | 同じ tick に Note On と Note Off を逆順で追加 | 書き出す | Note Off が先に書かれる |
| SMF-004 | テンポ 600000 µs、拍子 7/8 | 書き出す | `FF 51 03 09 27 C0`、`FF 58 04 07 03 18 08` |
| SMF-005 | どんなトラックでも | 書き出す | 末尾が `00 FF 2F 00` |
| MPF-001 | リポジトリの `midi_profiles/punai-punai-ensou.json` | 読み込む | 指揮者トラックなし、全トラックにテンポ、拍子なし、PPQ は resolution、曲頭基準 |
| MPF-002 | channel が 17 | 読み込む | `std::invalid_argument` |
| MPF-003 | track_per が "channel" | 読み込む | `std::invalid_argument`（未対応） |
| MPF-004 | 指揮者トラックも全トラックのテンポもなし | 読み込む | `std::invalid_argument`（テンポがどこにも入らない） |
