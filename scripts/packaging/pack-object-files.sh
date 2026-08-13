#!/bin/bash
# Pack the object files of a static enterprise build into a tar.gz so users
# can relink the static executables against a newer glibc (BUSL/static
# linking compliance). Runs right after "make install", inside the build
# image (it needs the
# static OpenSSL libraries from /opt).
#
# Archive layout: everything below the build directory
# appears under "build/" (link_executables.sh and README.static-linking
# rely on that name), plus lib/BuildId/BuildId.ld and the two helper files.
#
# Run from the repository root; requires:
#   BUILD_DIR        build directory of the preset (e.g. build-presets/nightly-package-x64)
#   BUILDMODE        CMake build type used            (default RelWithDebInfo)
#   PACKAGES_OUT     output directory                 (default ./packages)
# ARANGODB_VERSION is derived from CMakeLists.txt when not already exported.

set -euo pipefail

PROJECT_DIR="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_DIR="${BUILD_DIR:?BUILD_DIR must point to the CMake build directory}"
BUILDMODE="${BUILDMODE:-RelWithDebInfo}"
PACKAGES_OUT="${PACKAGES_OUT:-${PROJECT_DIR}/packages}"
ARCH="$(uname -m)"

if [ ! -d "${PROJECT_DIR}/${BUILD_DIR}" ]; then
  echo "pack-object-files: ${PROJECT_DIR}/${BUILD_DIR} does not exist" >&2
  exit 1
fi

if [ -z "${ARANGODB_VERSION:-}" ]; then
  source "${SCRIPT_DIR}/../ci/helpers.bash"
  find_arangodb_version "${PROJECT_DIR}/CMakeLists.txt" > /dev/null
fi

# V8 produces thin archives; rewrite them into self-contained ones so they
# survive being shipped.
while IFS= read -r lib; do
  echo "${lib} ..."
  ar -t "${lib}" | xargs ar rvs "${lib}.new" > /dev/null
  mv "${lib}.new" "${lib}"
done < <(find "${BUILD_DIR}/3rdParty/v8-build" -name "*.a" 2>/dev/null)

# The linking scripts reference ../../libssl.a / ../../libcrypto.a relative
# to the build directory.
cp -a "$(find /opt -name libssl.a | head -1)" "${BUILD_DIR}/"
cp -a "$(find /opt -name libcrypto.a | head -1)" "${BUILD_DIR}/"

INCLUSION_LIST="$(mktemp)"
trap 'rm -f "${INCLUSION_LIST}"' EXIT

find "${BUILD_DIR}" -name "*.a" > "${INCLUSION_LIST}"
for obj in arangovpack arangobackup arangobench arangosh arangodump \
           arangoexport arangorestore arangoimport arangod; do
  find "${BUILD_DIR}" -name "${obj}.cpp.o" >> "${INCLUSION_LIST}"
done
find "${BUILD_DIR}/client-tools" -name "*.cpp.o" >> "${INCLUSION_LIST}"
echo "lib/BuildId/BuildId.ld" >> "${INCLUSION_LIST}"

# ── Generate scripts/link_executables.sh from this very build ───────────────
# The relink script shipped in the archive used to be a hand-maintained
# snapshot of the link command lines; it rotted silently (stale object and
# library lists, an outdated compiler) because nothing ever executed it.
# It is now generated from the CMake link.txt files of the build being
# packed, so it always matches the object files next to it. Rewrites:
#   - the resolved compiler becomes the versioned clang++ users install
#     (major version taken from VERSIONS, same as the build toolchain)
#   - the /opt static OpenSSL paths become the libssl.a/libcrypto.a that
#     were copied into the build directory above
#   - absolute build-/source-tree paths become archive-relative ones
# Every referenced input is checked to exist at pack time; the nightly
# additionally relinks from the finished archive (see compile-nightly).
CLANG_MAJOR="$(grep -Po 'CLANG_LINUX "\K[0-9]+' "${PROJECT_DIR}/VERSIONS")"
python3 - "${PROJECT_DIR}" "${BUILD_DIR}" "${CLANG_MAJOR}" <<'PYEOF' > scripts/link_executables.sh
import os, shlex, sys

project, build_dir, clang_major = sys.argv[1], sys.argv[2], sys.argv[3]
build_abs = os.path.join(project, build_dir)
EXECUTABLES = ["arangod", "arangobench", "arangodump", "arangoexport",
               "arangoimport", "arangorestore", "arangosh", "arangovpack",
               "arangobackup"]

def fail(msg):
    print(f"pack-object-files: {msg}", file=sys.stderr)
    sys.exit(1)

def link_txt_of(exe):
    hits = []
    for root, _, files in os.walk(build_abs):
        if root.endswith(f"CMakeFiles/{exe}.dir") and "link.txt" in files:
            hits.append(os.path.join(root, "link.txt"))
    if len(hits) != 1:
        fail(f"expected exactly one link.txt for {exe}, found {hits}")
    return hits[0]

def transform(exe, tokens, subdir, up):
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
            tok = f"{up}/{os.path.basename(tok)}"
            orig = os.path.join(build_abs, os.path.basename(tok))
        elif tok.startswith(build_abs + "/"):
            tok = f"{up}/{tok[len(build_abs) + 1:]}"
            # orig stays absolute for the existence check
        elif tok.startswith(project + "/"):
            tok = f"{up}/../{tok[len(project) + 1:]}"
        if not is_output and not prefix and (tok.endswith((".a", ".o", ".ld")) or "/" in tok):
            path = orig if orig.startswith("/") else os.path.join(build_abs, subdir, orig)
            if not os.path.isfile(path):
                fail(f"{exe}: link line references missing input {orig}")
        out.append(prefix + tok)
    return out

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
    lt = link_txt_of(exe)
    subdir = os.path.relpath(os.path.dirname(os.path.dirname(os.path.dirname(lt))), build_abs)
    up = "/".join([".."] * len(subdir.split("/")))
    with open(lt) as f:
        tokens = shlex.split(f.read().replace("\n", " "))
    for tok in tokens:
        if tok.startswith("@"):
            fail(f"{exe}: response files are not supported: {tok}")
    cmd = transform(exe, tokens, subdir, up)
    print(f"\necho {exe}")
    print(f"( cd {subdir}")
    print("  " + " ".join(shlex.quote(t) for t in cmd))
    print(")")
PYEOF
chmod +x scripts/link_executables.sh

# The README's toolchain instructions follow the build's clang, like the
# generated script's.
sed -E "s/clang-[0-9]+/clang-${CLANG_MAJOR}/g; s/lld-[0-9]+/lld-${CLANG_MAJOR}/g" \
  "${SCRIPT_DIR}/README.static-linking" > README.static-linking
echo "scripts/link_executables.sh" >> "${INCLUSION_LIST}"
echo "README.static-linking" >> "${INCLUSION_LIST}"

mkdir -p "${PACKAGES_OUT}"
ARCHIVE="${PACKAGES_OUT}/arangodb3e-linux-object_files_${BUILDMODE}-${ARANGODB_VERSION}_${ARCH}.tar.gz"
rm -f "${ARCHIVE}"
tar -czf "${ARCHIVE}" \
  --transform "s|^${BUILD_DIR}|build|" \
  --files-from="${INCLUSION_LIST}"
ls -l "${ARCHIVE}"
