#!/usr/bin/env python3

# Copyright Hans Dembinski 2018-2019
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# script must be executed in project root folder

# NOTE: compute coverage with b2 toolset=gcc-8 cxxstd=latest coverage=on test//all
# - computing coverage only works properly with gcc-8 right now
# - gcc-9 and gcc-10 are extremely slow
# - clang-10 works and is fast but misses a lot of unmissable lines

from subprocess import run
from pathlib import Path
import os
import sys

LCOV_VERSION = "1.15"

gcov = os.environ.get("GCOV", "gcov")

gcov_version = gcov.split("-")[1] if "-" in gcov else None
gcc_version = f"gcc-{gcov_version}" if gcov_version else "gcc"

lcov_dir = Path("tools") / f"lcov-{LCOV_VERSION}"

if not lcov_dir.exists():
    url = (
        "https://github.com/linux-test-project/lcov/releases/download/"
        + f"v{LCOV_VERSION}/lcov-{LCOV_VERSION}.tar.gz"
    )
    run(f"curl -L {url} | tar zxf -", shell=True, cwd="tools")

# --rc lcov_branch_coverage=1 doesn't work on travis
lcov = [
    f"{lcov_dir}/bin/lcov",
    f"--gcov-tool={gcov}",
    "--output-file",
    "coverage.info",
]
# get all directories with gcda files that match the gcov version
cwd = Path().absolute()
lcov_collect = lcov + ["--base-directory", cwd, "--capture"]
for p in Path("../../bin.v2/libs/histogram/test").rglob("**/*.gcda"):
    if gcc_version not in str(p):
        continue
    lcov_collect.append("--directory")
    lcov_collect.append(p.parent)
run(lcov_collect)

# remove uninteresting entries
run(lcov + ["--extract", "coverage.info", "*/boost/histogram/*"])

args = sys.argv[1:]
if args:
    # upload if token is passed as argument, you need to install cpp-coveralls
    run(["cpp-coveralls", "-l", "coverage.info", "-r", "../..", "-n", "-t", args[0]])
else:
    # otherwise generate html report
    run(
        [
            f"{lcov_dir}/bin/genhtml",
            "coverage.info",
            "--demangle-cpp",
            "-o",
            "coverage-report",
        ]
    )
