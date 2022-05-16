#!/usr/bin/env python3

# Copyright 2019 Hans Dembinski
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

import subprocess as subp
from pathlib import Path
from multiprocessing.pool import ThreadPool

clang_tidy_cmd = None
for version in range(15, 5, -1):
    clang_tidy_cmd = f"clang-tidy-{version}"
    if subp.run(("which", clang_tidy_cmd), stdout=subp.DEVNULL).returncode == 0:
        break

project_dir = Path(__file__).resolve().parents[1]
assert project_dir.exists()
boost_dir = project_dir.parents[1]

filenames = (project_dir / "include").rglob("*.hpp")


def run_tidy(filename):
    n = len(project_dir.parts) + 2
    cmd = f"{clang_tidy_cmd} {filename} -- -I{boost_dir}"
    return (
        cmd,
        subp.run(cmd.split(), stdout=subp.PIPE, stderr=subp.STDOUT).stdout.decode(
            "utf-8"
        ),
    )


pool = ThreadPool()
for cmd, report in pool.map(run_tidy, filenames):
    if report:
        print(cmd)
        print(report)
