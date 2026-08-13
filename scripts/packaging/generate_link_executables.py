#!/usr/bin/env python3
"""Generate the link_executables.sh shipped inside the object-files archive.

Invoked by pack-object-files.sh as
    generate_link_executables.py <project-dir> <build-dir> <clang-major>
and writes the shell script to stdout.

The generated script is the artifact end users run to relink the shipped
object files against their own (newer) glibc — the LGPL compliance promise
of the statically linked executables. It used to be a hand-maintained
snapshot of the link command lines, which rotted silently (stale object and
library lists, an outdated compiler) because nothing ever executed it; it
is therefore generated from the link command lines of the very build being
packed, so it always matches the object files next to it.

The link commands are read from the build tree:
  - Ninja (what the nightly passes via -G Ninja): "ninja -t commands
    bin/<exe>", whose last line is the link edge, wrapped by CMake as
    ": && <link command> && :". Commands run from the build root.
  - Unix Makefiles (the preset default): CMakeFiles/<exe>.dir/link.txt,
    executed from the target's binary directory.

Rewrites applied to each command line:
  - the resolved compiler becomes the versioned clang++ users can install
    (major version from VERSIONS, the same toolchain the build used)
  - the /opt static OpenSSL paths become the libssl.a/libcrypto.a that
    pack-object-files.sh copies into the build directory
  - absolute build-/source-tree paths become archive-relative ones

Every referenced input is checked to exist at pack time, so drift fails
the packaging step; the nightly additionally relinks from the finished
archive (see the compile-nightly job).
"""

import os
import shlex
import subprocess
import sys

EXECUTABLES = [
    "arangod",
    "arangobench",
    "arangodump",
    "arangoexport",
    "arangoimport",
    "arangorestore",
    "arangosh",
    "arangovpack",
    "arangobackup",
]


def fail(msg):
    print(f"generate_link_executables: {msg}", file=sys.stderr)
    sys.exit(1)


def find_link_txts(build_abs):
    """One walk over the build tree collecting CMakeFiles/<exe>.dir/link.txt
    for every executable (a separate walk per executable proved slow on
    large build directories)."""
    wanted = set(EXECUTABLES)
    hits = {exe: [] for exe in EXECUTABLES}
    for root, _, files in os.walk(build_abs):
        if "link.txt" not in files:
            continue
        name = os.path.basename(root)
        if not name.endswith(".dir"):
            continue
        if os.path.basename(os.path.dirname(root)) != "CMakeFiles":
            continue
        exe = name[: -len(".dir")]
        if exe in wanted:
            hits[exe].append(os.path.join(root, "link.txt"))
    return hits


def makefiles_tokens(link_txt):
    with open(link_txt) as f:
        return shlex.split(f.read().replace("\n", " "))


def ninja_tokens(build_abs, exe):
    """The link edge of bin/<exe>: the last command 'ninja -t commands'
    prints, unwrapped from CMake's ': && <link> && :' chain."""
    result = subprocess.run(
        ["ninja", "-C", build_abs, "-t", "commands", f"bin/{exe}"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        fail(f"{exe}: ninja -t commands failed: {result.stderr.strip()}")
    lines = result.stdout.strip().splitlines()
    if not lines:
        fail(f"{exe}: ninja -t commands returned no commands for bin/{exe}")
    segments = [s.strip() for s in lines[-1].split("&&")]
    candidates = [s for s in segments if "clang++" in s.split(" ", 1)[0]]
    if len(candidates) != 1:
        fail(f"{exe}: expected exactly one clang++ link segment, got: {lines[-1]}")
    return shlex.split(candidates[0])


def transform(exe, tokens, subdir, up, project, build_abs, clang_major):
    out, skip_check = [], False
    if "clang++" not in tokens[0]:
        fail(f"{exe}: link command does not start with clang++: {tokens[0]}")
    out.append(f"clang++-{clang_major}")
    for tok in tokens[1:]:
        is_output = skip_check
        skip_check = tok == "-o"
        prefix = ""
        if tok.startswith("-L"):
            prefix, tok = "-L", tok[2:]
        if not tok or (tok.startswith("-") and not prefix):
            out.append(tok)
            continue
        orig = tok
        if tok.startswith("/") and tok.endswith(("libssl.a", "libcrypto.a")) and "/opt/" in tok + "/":
            tok = os.path.normpath(f"{up}/{os.path.basename(tok)}")
            orig = os.path.join(build_abs, os.path.basename(tok))
        elif tok.startswith(build_abs + "/"):
            tok = os.path.normpath(f"{up}/{tok[len(build_abs) + 1:]}")
            # orig stays absolute for the existence check
        elif tok.startswith(project + "/"):
            tok = os.path.normpath(f"{up}/../{tok[len(project) + 1:]}")
        if not is_output and not prefix and (tok.endswith((".a", ".o", ".ld")) or "/" in tok):
            path = orig if orig.startswith("/") else os.path.join(build_abs, subdir, orig)
            if not os.path.isfile(path):
                fail(f"{exe}: link line references missing input {orig}")
        out.append(prefix + tok)
    return out


def main():
    if len(sys.argv) != 4:
        fail(f"usage: {sys.argv[0]} <project-dir> <build-dir> <clang-major>")
    project, build_dir, clang_major = sys.argv[1], sys.argv[2], sys.argv[3]
    if not clang_major.isdigit():
        fail(f"clang major version must be numeric, got {clang_major!r}")
    build_abs = os.path.join(project, build_dir)
    if not os.path.isdir(build_abs):
        fail(f"build directory {build_abs} does not exist")

    link_txts = find_link_txts(build_abs)
    use_ninja = not any(link_txts.values())
    if use_ninja and not os.path.isfile(os.path.join(build_abs, "build.ninja")):
        fail(f"neither link.txt files nor build.ninja found in {build_abs}")
    if not use_ninja:
        for exe, found in link_txts.items():
            if len(found) != 1:
                fail(f"expected exactly one link.txt for {exe}, found {found}")

    print(f"""#!/bin/bash
# GENERATED at packaging time by scripts/packaging/pack-object-files.sh
# from the very build this archive was created from - do not edit.
#
# Relinks the shipped object files into the static executables
# ({", ".join(EXECUTABLES)}),
# so they can be rebuilt against your own (newer) glibc.
# Use Ubuntu 24.04 (with glibc 2.39 or later) and install:
#   apt install build-essential clang-{clang_major} lld-{clang_major} liburing-dev
# Execute in the directory in which you extracted the archive!
set -e
cd build""")

    for exe in EXECUTABLES:
        if use_ninja:
            subdir, up = ".", "."
            tokens = ninja_tokens(build_abs, exe)
        else:
            lt = link_txts[exe][0]
            subdir = os.path.relpath(os.path.dirname(os.path.dirname(os.path.dirname(lt))), build_abs)
            up = "." if subdir == "." else "/".join([".."] * len(subdir.split("/")))
            tokens = makefiles_tokens(lt)
        for tok in tokens:
            if tok.startswith("@"):
                fail(f"{exe}: response files are not supported: {tok}")
        cmd = transform(exe, tokens, subdir, up, project, build_abs, clang_major)
        print(f"\necho {exe}")
        if subdir == ".":
            print(" ".join(shlex.quote(t) for t in cmd))
        else:
            print(f"( cd {subdir}")
            print("  " + " ".join(shlex.quote(t) for t in cmd))
            print(")")


if __name__ == "__main__":
    main()
