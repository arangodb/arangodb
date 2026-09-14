#!/usr/bin/env python3
"""Summarize a clang-tidy run: where the time went and what was found.

Reads the captured output of utils/clang-tidy.sh (as produced by run-clang-tidy)
and prints per-translation-unit timings and a breakdown of the findings. Used by
the CI job to size -j and the resource class; also useful locally.

Usage: clang-tidy-analyze.py <clang-tidy-output.txt>
"""

import collections
import re
import sys

# run-clang-tidy progress line; the time is that file's own duration.
#   [ 192/1577][51.3s] clang-tidy-19 -p=/path -quiet /root/project/foo.cpp
PROGRESS = re.compile(r"^\[ *\d+/\d+\]\[([\d.]+)s\].* (\S+\.(?:cpp|cc|c))$")

# A reported diagnostic. Trailing [check] may list several comma-separated names.
DIAGNOSTIC = re.compile(
    r"^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): "
    r"(?:warning|error): (?P<msg>.*?) \[(?P<checks>[A-Za-z][\w.,-]*)\]$"
)

BUCKETS = [(5, "<5s"), (15, "5-15s"), (30, "15-30s"), (60, "30-60s")]


def main(path):
    times = []
    findings = set()
    by_check = collections.Counter()
    by_file = collections.Counter()

    with open(path, errors="replace") as handle:
        for raw in handle:
            line = raw.rstrip("\n")

            progress = PROGRESS.match(line)
            if progress:
                times.append((float(progress.group(1)), progress.group(2)))
                continue

            diag = DIAGNOSTIC.match(line)
            if not diag:
                continue
            # The same header is re-analyzed by every TU including it, so the
            # identical finding arrives many times. Count it once.
            key = (diag["file"], diag["line"], diag["col"], diag["checks"])
            if key in findings:
                continue
            findings.add(key)
            by_file[diag["file"]] += 1
            for check in diag["checks"].split(","):
                by_check[check] += 1

    print("\n=== per-file analysis time ===")
    if times:
        total = sum(t for t, _ in times)
        print(
            f"files={len(times)}  total={total:.0f}s ({total / 3600:.1f} CPU-h)  "
            f"mean={total / len(times):.1f}s  max={max(times)[0]:.1f}s"
        )
        print("\n--- 25 slowest translation units ---")
        for seconds, name in sorted(times, reverse=True)[:25]:
            print(f"{seconds:8.1f}s  {name}")

        print("\n--- time buckets ---")
        counts = collections.Counter()
        for seconds, _ in times:
            counts[next((n for lim, n in BUCKETS if seconds < lim), ">60s")] += 1
        for _, name in BUCKETS + [(0, ">60s")]:
            print(f"  {name}: {counts[name]}")
    else:
        print("(no progress lines found)")

    print(f"\n=== findings: {len(findings)} unique ===")
    if by_check:
        print("--- by check ---")
        for check, count in by_check.most_common(40):
            print(f"{count:8d}  {check}")
        print("\n--- 25 files with most findings ---")
        for name, count in by_file.most_common(25):
            print(f"{count:8d}  {name}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
