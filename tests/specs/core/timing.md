# タイミング変換 (core/timing)

- 対象: `ibs::core::TimingMap`
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
