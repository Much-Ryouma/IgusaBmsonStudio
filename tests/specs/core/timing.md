# タイミング変換 (core/timing)

- 対象: `ibs::core::TimingMap`（`secondsAt` / `pulseAt` / `bpmAt`）
- テストコード: `tests/unit/core/timing/TimingMapTest.cpp`
- 根拠: bmson 仕様「Timing」「Order of Processing」、SPEC.md §4.1（resolution の保持）

## 何を保証するか

bmson の位置（pulse）を、曲頭からの経過時間（秒）に正しく変換できること。
BPM 変化と STOP を、bmson 仕様の処理順（ノーツ → BPM → STOP）どおりに扱えること。

## テスト項目

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| TIM-001 | init_bpm = 120, resolution = 240 | pulse 240 を秒に変換 | 0.5 秒 |
| TIM-002 | init_bpm = 120, resolution = 240, pulse 480 で BPM 60 に変化 | pulse 720 を秒に変換 | 2.0 秒（480 までが 1.0 秒、そこから 240 pulse が 1.0 秒） |
| TIM-003 | init_bpm = 60, resolution = 240, pulse 240 に STOP 240 pulse | pulse 240 を秒に変換 | 1.0 秒（STOP と同じ位置は停止の開始時刻） |
| TIM-004 | TIM-003 と同じ | pulse 241 を秒に変換 | 2.0 + 1/240 秒（停止の 1.0 秒が加算される） |
| TIM-005 | init_bpm = 60, pulse 240 に BPM 100 と BPM 120 の2つ（この順） | pulse 240 の BPM を求める | 120（同じ位置では最後の BPM が有効） |
| TIM-006 | init_bpm = 60, resolution = 240, pulse 240 に STOP 240 と STOP 960 | pulse 480 を秒に変換 | 7.0 秒（停止が合計 1200 pulse = 5.0 秒） |
| TIM-007 | init_bpm = 60, resolution = 240, pulse 240 に BPM 120 と STOP 240 | pulse 480 を秒に変換 | 2.0 秒（停止は変化後の BPM 120 で 0.5 秒） |
| TIM-008 | init_bpm = 100, resolution = 96（完成品の例と同じ設定） | pulse 384（1小節）を秒に変換 | 2.4 秒 |
| TIM-009 | BPM 変化を pulse の降順で渡す | 各位置を秒に変換 | 昇順で渡したときと同じ結果 |
| TIM-010 | init_bpm = 0、または resolution = 0 | TimingMap を作る | `std::invalid_argument` を投げる |
| TIM-011 | BPM 変化の bpm が負、または位置が負 | TimingMap を作る | `std::invalid_argument` を投げる |
| TIM-012 | init_bpm = 120, resolution = 240, pulse 480 で BPM 60 に変化 | 2.0 秒を pulse に変換 | 720（`secondsAt` の逆変換になる） |
| TIM-013 | init_bpm = 60, resolution = 240, pulse 240 に STOP 240 pulse | 1.0〜2.0 秒を pulse に変換 | すべて 240（停止中は譜面が進まない） |
| TIM-014 | TIM-013 と同じ | 2.5 秒を pulse に変換 | 360 |

## タイミングの編集 (TED)

- 対象: `setBpmChange` / `setStop` / `setTimeSignature` / `removeTimeSignatureChange` / `insertBarLine` / `removeBarLine` / `barIndexAt` / `barStartPulse`（`tests/unit/core/timing/TimingEditsTest.cpp`）
- 根拠: SPEC.md §6.4（タイミング、拍子トラックから lines を生成）
- 拍子は「小節の長さの変化点」（`BarLengthChange`）として lines から読み取り、編集後に lines を作り直す。拍子を別に保存しないのは、bmson には lines しかなく、読み込んだ譜面でも同じように編集できるようにするため

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| TED-001 | 120 BPM | 960 に 150 → 480 に 140 → 960 を 160 に → 0 に 130 / 0 BPM / 480 を削除して再度削除 | 480:140、960:160 の順。0 の指定は init_bpm が 130 になる / `std::invalid_argument` / 1 つになる → `std::invalid_argument` |
| TED-002 | 120 BPM | 960 に 120 → 480 に 60 → 960 を 240 に / 長さ 0 / 480 を削除して再度削除 | 480:60、960:240 の順 / `std::invalid_argument` / 2 回目は `std::invalid_argument` |
| TED-003 | resolution 240、小節線なし（4/4） | 3 小節目（bar 2）から 7/8、5 小節目（bar 4）から 4/4（曲の終わり 4000） | 小節線 0, 960, 1920, 2760, 3600, 4560 …（7/8 は 840）。変化点は (0:960)(2:840)(4:960)。曲の終わりより先まで作る |
| TED-004 | TED-003 の後 | bar 4 の変化を消す / bar 0 を消す / 変化のない bar 3 を消す | 7/8 が最後まで続く / `std::invalid_argument`（最初の拍子は変更で直す）/ `std::invalid_argument` |
| TED-005 | resolution 5 | 7/8 を指定 / resolution 240 の 5/4 の長さ | 1 小節が整数の pulse にならないので `std::invalid_argument` / 1200 |
| TED-006 | 小節線なし（4/4） | 2400 に小節線を追加 / 960 に追加 / 2400 を削除して再度削除 / 0 を削除 | 0, 960, 1920, 2400, 2880 …（それより前の 4/4 の小節線は残る）/ すでにあるので `std::invalid_argument` / 2 回目は `std::invalid_argument` / 曲頭は消せない |
| TED-007 | 小節線 0, 960, 1800 | 0 / 1799 / 1800 / 2640 の小節番号、bar 1 / bar 3 の頭 | 0 / 1 / 2 / 3（最後の小節線の後は 840 ごと）、960 / 2640 |
