#!/usr/bin/env python3
"""
Run clang-tidy, capture output to a log file, and compute a simple 0..10 score.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path


ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
DIAG_RE = re.compile(r":\s*(warning|error):")


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text)


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(value, high))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--clang-tidy", required=True, dest="clang_tidy")
    parser.add_argument("--build-dir", required=True, dest="build_dir")
    parser.add_argument("--source-dir", required=True, dest="source_dir")
    parser.add_argument("--output", required=True, dest="output")
    parser.add_argument("--score-out", required=True, dest="score_out")
    parser.add_argument("clang_tidy_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    tidy_args = list(args.clang_tidy_args)
    if tidy_args and tidy_args[0] == "--":
        tidy_args = tidy_args[1:]

    cmd = [args.clang_tidy, "-p", args.build_dir] + tidy_args

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    warning_count = 0
    error_count = 0

    with output_path.open("w", encoding="utf-8") as log_file:
        proc = subprocess.Popen(  # noqa: S603
            cmd,
            cwd=args.source_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        assert proc.stdout is not None
        for raw_line in proc.stdout:
            sys.stdout.write(raw_line)
            log_file.write(raw_line)

            clean_line = strip_ansi(raw_line)
            diag_match = DIAG_RE.search(clean_line)
            if not diag_match:
                continue
            if diag_match.group(1) == "warning":
                warning_count += 1
            else:
                error_count += 1

        return_code = proc.wait()

    # Simple and stable scoring model:
    # - 1.0 penalty per error
    # - 0.05 penalty per warning
    # Score clamped to [0, 10]
    score = clamp(10.0 - (1.0 * error_count) - (0.05 * warning_count), 0.0, 10.0)

    score_text = (
        f"clang-tidy score: {score:.2f}/10 "
        f"(errors={error_count}, warnings={warning_count}, "
        "formula=10-1.0*errors-0.05*warnings)\n"
    )
    Path(args.score_out).write_text(score_text, encoding="utf-8")
    sys.stdout.write(score_text)

    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
