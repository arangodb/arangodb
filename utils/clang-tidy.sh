#!/usr/bin/env bash
# Run clang-tidy over ArangoDB source files.
#
# pre-requisite: Linux-based system, Bash 4, Git 2.9+, jq
# pre-requisite: clang-tidy + run-clang-tidy on PATH (the pinned toolchain
#                ships clang-tidy 19.1.7; a matching version is strongly
#                recommended so the C++20 dialect is parsed identically)
# pre-requisite: a CONFIGURED and BUILT build directory containing
#                compile_commands.json (clang-tidy reads the exact compile
#                flags from there; it does not run the code)
#
# Usage:
#   ./utils/clang-tidy.sh [options] [file-or-dir ...]
#
# With no file arguments, checks the files locally modified vs. HEAD in both
# the main and the enterprise repository (same selection as clang-format.sh).
# With arguments, checks exactly those files (directories are expanded to their
# C/C++ source files).
#
# Only translation units (.cpp/.cc/.c) are handed to clang-tidy; headers are
# analyzed transitively via HeaderFilterRegex in .clang-tidy when a TU that
# includes them is checked. Files under 3rdParty/ are skipped.
#
# Options:
#   -B, --build DIR   build directory with compile_commands.json (default: ./build)
#   -j N              parallel jobs (default: number of CPUs)
#       --fix         apply clang-tidy's suggested fixes in place
#   -h, --help        show this help and exit

set -euo pipefail

adb_path="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

build_dir="build"
jobs="$(nproc 2>/dev/null || echo 4)"
fix=""
files_from_args=()

usage() { sed -n '2,28p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    -B|--build) build_dir="$2"; shift 2 ;;
    --build=*)  build_dir="${1#*=}"; shift ;;
    -j)         jobs="$2"; shift 2 ;;
    -j*)        jobs="${1#-j}"; shift ;;
    --fix)      fix="-fix"; shift ;;
    -h|--help)  usage; exit 0 ;;
    --)         shift; while [[ $# -gt 0 ]]; do files_from_args+=("$1"); shift; done ;;
    -*)         echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    *)          files_from_args+=("$1"); shift ;;
  esac
done

for tool in clang-tidy run-clang-tidy jq; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: '$tool' not found on PATH." >&2
    exit 1
  fi
done

cd "$adb_path"

# Resolve the build directory and its compilation database.
if [[ "$build_dir" != /* ]]; then
  build_dir="$adb_path/$build_dir"
fi
db="$build_dir/compile_commands.json"

if [[ ! -f "$db" ]]; then
  echo "error: no compile_commands.json in build directory:" >&2
  echo "         $build_dir" >&2
  echo "       Configure a build there, or point at another one with --build DIR." >&2
  echo "       (This checkout typically builds in cmake-build-relwithdebinfo/ —" >&2
  echo "        e.g. ./utils/clang-tidy.sh --build cmake-build-relwithdebinfo)" >&2
  exit 1
fi
echo "Build directory: $build_dir"

# ---------------------------------------------------------------------------
# Staleness guards. clang-tidy is only as correct as the compile database it
# reads; a database from the "wrong" or an un-refreshed build dir produces
# confusing parse errors. We warn (but do not hard-fail) on the common cases.
# ---------------------------------------------------------------------------

# (c) Was the build ever actually built? Several headers are generated at build
#     time (voc-errors.h, error-registry.h, exitcodes.h). If they are absent,
#     even DB-listed TUs will fail to parse. A missing DB entry (guard b) would
#     also surface this, but this message is more actionable.
if [[ -z "$(find "$build_dir" -name voc-errors.h -print -quit 2>/dev/null)" ]]; then
  echo "warning: generated headers (e.g. voc-errors.h) not found under the build" >&2
  echo "         directory. If you have only configured but not built it, run a" >&2
  echo "         build first (at least the 'errorfiles'/'exitcodefiles' targets)," >&2
  echo "         otherwise clang-tidy will fail to parse many files." >&2
fi

# (a) Has cmake config changed since the DB was generated? The DB is usually
#     regenerated on the next build, so this is a soft heads-up, not an error.
if find . -path ./3rdParty -prune -o \
       \( -name CMakeLists.txt -o -name '*.cmake' \) -newer "$db" -print 2>/dev/null \
     | grep -q .; then
  echo "warning: a CMakeLists.txt / *.cmake is newer than compile_commands.json." >&2
  echo "         The compile database may be stale; consider re-running cmake" >&2
  echo "         (or a build, which regenerates it) before relying on results." >&2
fi

# ---------------------------------------------------------------------------
# Collect the list of source files to check.
# ---------------------------------------------------------------------------
src_re='\.\(cpp\|cc\|c\)$'   # translation units only (grep BRE)

collect_from_git() {
  # Locally modified (unstaged + staged) sources in the main repo ...
  git diff  --diff-filter=ACMRT --name-only      -- arangod/ lib/ client-tools/ tests/
  git diff  --diff-filter=ACMRT --name-only HEAD -- arangod/ lib/ client-tools/ tests/
  # ... and in the nested enterprise repo, path-prefixed so they resolve here.
  if [[ -d enterprise ]]; then
    ( cd enterprise \
      && git diff --diff-filter=ACMRT --name-only      -- Enterprise tests/ \
      && git diff --diff-filter=ACMRT --name-only HEAD -- Enterprise tests/ \
    ) | sed -e 's#^#enterprise/#'
  fi
}

declare -a files=()
if [[ ${#files_from_args[@]} -gt 0 ]]; then
  for f in "${files_from_args[@]}"; do
    if [[ -d "$f" ]]; then
      while IFS= read -r found; do files+=("$found"); done \
        < <(find "$f" -type f | grep "$src_re")
    else
      files+=("$f")
    fi
  done
else
  echo "No files given; selecting sources changed vs. HEAD."
  while IFS= read -r found; do files+=("$found"); done \
    < <(collect_from_git | grep "$src_re" || true)
fi

# Normalise: drop 3rdParty, keep source TUs only, unique.
declare -a sources=()
while IFS= read -r f; do
  [[ -n "$f" ]] && sources+=("$f")
done < <(printf '%s\n' "${files[@]}" | grep -v '^3rdParty/' | grep "$src_re" | sort -u || true)

if [[ ${#sources[@]} -eq 0 ]]; then
  echo "Nothing to check (no matching C/C++ source files)."
  exit 0
fi

# (b) Verify each selected source is present in the compile database. A missing
#     entry almost always means the wrong build dir, or cmake hasn't been re-run
#     since the file was added. This is the guard that catches "stale ./build".
#
#     The database is parsed ONCE (with jq) into a set of repo-relative paths,
#     then each source is a hash lookup -- rather than one grep over the
#     (16 MB+) DB per file, which is quadratic and dominates the runtime on a
#     full-tree run. Each entry's path is resolved against its "directory" in
#     case the "file" field is relative, then made repo-relative.
declare -A in_db=()
while IFS= read -r p; do
  [[ -n "$p" ]] && in_db["${p#"${adb_path}/"}"]=1
done < <(
  jq -r '.[] | if (.file | startswith("/")) then .file
               else .directory + "/" + .file end' "$db"
)

declare -a present=()
missing=0
for f in "${sources[@]}"; do
  if [[ -n "${in_db[$f]:-}" ]]; then
    present+=("$f")
  else
    if [[ $missing -eq 0 ]]; then
      echo "warning: the following files are NOT in $db" >&2
      echo "         and will be skipped. Re-run cmake, or check --build points" >&2
      echo "         at the build directory you actually use:" >&2
    fi
    echo "           $f" >&2
    missing=1
  fi
done

if [[ ${#present[@]} -eq 0 ]]; then
  echo "error: none of the selected files are in the compile database; aborting." >&2
  exit 1
fi

echo
echo "About to run clang-tidy on the following files:"
printf '%s\n' "${present[@]}" | cat -n
echo

# run-clang-tidy matches its positional args as regexes against DB paths.
# Passing the exact relative paths is specific enough in practice.
run-clang-tidy -p "$build_dir" -j "$jobs" -quiet $fix "${present[@]}"
