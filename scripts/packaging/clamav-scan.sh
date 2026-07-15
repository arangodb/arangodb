#!/bin/bash
# Scan built packages with ClamAV.
#
# Usage: clamav-scan.sh <packages-directory>...
#   Directories that do not exist are skipped.
#
# Every regular file (except *.clamav and *.asc) is unpacked into a sandbox
# and scanned with clamscan. A <file>.clamav report is written next to each
# file; the script exits non-zero if any infected file is found or a scan
# fails. The virus database is expected to be present already (freshclam is
# run by the CI job, with the database cached in object storage).

set -uo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: clamav-scan.sh <packages-directory>..." >&2
  exit 1
fi
SCAN_DIRS=()
for dir in "$@"; do
  [ -d "${dir}" ] && SCAN_DIRS+=("${dir}")
done
if [ "${#SCAN_DIRS[@]}" -eq 0 ]; then
  echo "clamav-scan: none of the given directories exist, nothing to scan"
  exit 0
fi
SANDBOX="$(mktemp -d)"
trap 'rm -rf "${SANDBOX}"' EXIT

infected=0

unpack() {
  local filename="$1"

  rm -rf "${SANDBOX}/work"
  mkdir -p "${SANDBOX}/work"

  case "${filename}" in
    *.tar.gz|*.tar|*.zip|*.exe|*.dmg|*.html)
      cp "${filename}" "${SANDBOX}/work" || return 1
      ;;
    *.deb)
      (cd "${SANDBOX}/work" && ar x "${filename}") || return 1
      ;;
    *.rpm)
      (cd "${SANDBOX}/work" && rpm2cpio "${filename}" | cpio -i -d --quiet) || return 1
      ;;
    *)
      echo "FATAL: unknown file type in '${filename}'" >&2
      return 1
      ;;
  esac
}

scan() {
  local filename="$1"
  local report="${filename}.clamav"
  local sha
  sha="$(sha256sum "${filename}" | awk '{print $1}')"

  echo "================================================================================"
  echo "Scanning ${filename}"

  if ! unpack "${filename}"; then
    return 1
  fi

  {
    echo "${sha}"
    echo
    echo "Filename:  $(basename "${filename}")"
    echo "Date:  $(date)"
    echo
  } > "${report}"
  # --max-filesize must be raised alongside --max-scansize: it defaults
  # to 25M and any larger file would be silently SKIPPED, still
  # reporting "Infected files: 0". 2000M is clamscan's upper limit.
  clamscan -r -v --max-scansize=2000M --max-filesize=2000M --max-recursion=10 \
    "${SANDBOX}/work" >> "${report}"
  local scan_status=$?
  rm -rf "${SANDBOX}/work"
  # 0 = clean, 1 = infected (counted from the report below), > 1 = error
  if [ "${scan_status}" -gt 1 ]; then
    echo "FATAL: clamscan failed for ${filename}" >&2
    return 1
  fi

  if ! grep -F "Infected files:" "${report}"; then
    echo "FATAL: scanning failed, 'Infected files' line not found" >&2
    return 1
  fi

  infected=$((infected + $(awk '/^Infected files:/ {print $3}' "${report}")))
}

status=0
while IFS= read -r file; do
  if ! scan "${file}"; then
    status=1
  fi
done < <(find "${SCAN_DIRS[@]}" -type f ! -name "*.clamav" ! -name "*.asc" ! -name "sourceInfo.*" | grep -Fv .revoked | sort)

echo "Infected files: ${infected}"

if [ "${infected}" -gt 0 ]; then
  exit 1
fi
exit "${status}"
