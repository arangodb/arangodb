#!/usr/bin/env python3
"""
Collects the CTestTestfile.cmake files needed to run the given ctests listed
in <ctest-yml-file>.

CTestTestfile.cmake exists in every directory CMake processed via
add_subdirectory(), even ones with no tests of their own (a few hundred
files project-wide) - ctest still needs each one that lies on the path down
to a test it's asked to run, but tolerates missing sibling subdirs it never
has to traverse. This script lists only the files required to execute the
given ctests.

Writes the resulting paths (relative to <build-dir>'s parent, one per line)
to <output-file-list>.

Usage: collect_ctest_metadata.py <build-dir> <ctest-yml-file> <output-file-list>
"""
from itertools import accumulate
from pathlib import Path
import sys

import yaml


def test_names(ctest_yml_path):
    with open(ctest_yml_path) as f:
        return [
            name
            for entry in yaml.safe_load(f)
            for name in next(iter(entry.values())).get("suites", [])
        ]


def find_defining_files(build_dir, name):
    """All CTestTestfile.cmake files under build_dir that define a test named `name`.

    Exits with an error if none define it.

    [=[<name>]=] is cmake's declaration pattern for a test
    """
    matches = [
        path
        for path in build_dir.rglob("CTestTestfile.cmake")
        if f"[=[{name}]=]" in path.read_text(errors="ignore")
    ]
    if not matches:
        sys.exit(
            f"ERROR: no CTestTestfile.cmake in {build_dir} defines a test named '{name}'"
        )
    return matches


def ancestor_chain(build_dir, file_path):
    """The CTestTestfile.cmake files from build_dir down to file_path's directory.

    This is the chain ctest needs on disk to reach file_path's test.
    """
    return [
        path / "CTestTestfile.cmake"
        for path in accumulate(
            file_path.relative_to(build_dir).parent.parts,
            lambda ancestor, part: ancestor / part,
            initial=build_dir,
        )
    ]


def main():
    build_dir = Path(sys.argv[1])

    needed = {
        path
        for name in test_names(sys.argv[2])
        for match in find_defining_files(build_dir, name)
        for path in ancestor_chain(build_dir, match)
    }

    with open(sys.argv[3], "w") as f:
        f.writelines(f"{path}\n" for path in sorted(needed))


if __name__ == "__main__":
    main()
