"""Compare automatically detected cut points with the cut points of a finished bmson chart.

Usage:
    python tools/evaluate_slicing.py <igusa-cli> <chart.bmson> <stem-folder> [extra slice options...]

For every sound channel of the chart whose stem (same name, any of .wav/.ogg/.flac) exists in the
stem folder, run `igusa-cli slice --chart <chart> --json` and match the detected cuts with the
chart's cuts within +-15 ms. The chart's cuts are the author's choices, not ground truth, so the
score is a guide for tuning rather than an accuracy measurement.
"""

import json
import os
import statistics
import subprocess
import sys

TOLERANCE_SECONDS = 0.015
STEM_EXTENSIONS = (".wav", ".ogg", ".flac")


def seconds_of(pulse, resolution, bpm):
    return pulse * 60.0 / bpm / resolution


def count_matches(expected, actual):
    used = set()
    matched = 0
    for value in expected:
        best = None
        for index, candidate in enumerate(actual):
            if index in used or abs(value - candidate) > TOLERANCE_SECONDS:
                continue
            if best is None or abs(value - candidate) < abs(value - actual[best]):
                best = index
        if best is not None:
            used.add(best)
            matched += 1
    return matched


def find_stem(folder, channel_name):
    stem = os.path.splitext(channel_name)[0]
    for extension in STEM_EXTENSIONS:
        path = os.path.join(folder, stem + extension)
        if os.path.exists(path):
            return path
    return None


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    cli, chart_path, stem_folder, extra = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
    with open(chart_path, encoding="utf-8-sig") as chart_file:
        chart = json.load(chart_file)
    if chart.get("bpm_events") and any(event["bpm"] != chart["info"]["init_bpm"] for event in chart["bpm_events"]):
        print("note: tempo changes are ignored when converting to seconds for matching")
    resolution = chart["info"].get("resolution") or 240
    bpm = chart["info"]["init_bpm"]

    print(f"{'stem':16s} {'chart':>5s} {'det':>5s} {'prec':>5s} {'rec':>5s} {'F':>5s} {'onset-chart[ms]':>15s}")
    total_chart = total_detected = total_matched = 0
    for channel in chart["sound_channels"]:
        stem = find_stem(stem_folder, channel["name"])
        if stem is None:
            continue
        chart_cuts = sorted({note["y"] for note in channel["notes"] if note["y"] > 0})
        completed = subprocess.run([cli, "slice", stem, "--chart", chart_path, "--json", *extra],
                                   capture_output=True, text=True, check=True)
        result = json.loads(completed.stdout)
        detected = sorted(cut["y"] for cut in result["cuts"])

        expected_seconds = [seconds_of(y, resolution, bpm) for y in chart_cuts]
        detected_seconds = [seconds_of(y, resolution, bpm) for y in detected]
        matched = count_matches(expected_seconds, detected_seconds)
        offsets = []
        for cut in result["cuts"]:
            onset = cut["onset_seconds"]
            nearest = min(expected_seconds, key=lambda value: abs(value - onset), default=None)
            if nearest is not None and abs(nearest - onset) <= 2 * TOLERANCE_SECONDS:
                offsets.append((onset - nearest) * 1000.0)

        precision = matched / len(detected) if detected else 0.0
        recall = matched / len(chart_cuts) if chart_cuts else 0.0
        f_score = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
        median = statistics.median(offsets) if offsets else float("nan")
        print(f"{os.path.basename(stem):16s} {len(chart_cuts):5d} {len(detected):5d} "
              f"{precision:5.2f} {recall:5.2f} {f_score:5.2f} {median:15.1f}")
        total_chart += len(chart_cuts)
        total_detected += len(detected)
        total_matched += matched

    if total_detected and total_chart:
        precision = total_matched / total_detected
        recall = total_matched / total_chart
        f_score = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
        print(f"TOTAL chart={total_chart} detected={total_detected} P={precision:.2f} R={recall:.2f} F={f_score:.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
