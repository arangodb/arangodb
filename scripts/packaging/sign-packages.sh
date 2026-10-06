#!/bin/bash
# Detach-sign package files with GPG, producing <file>.asc next to each file.
#
# Usage: sign-packages.sh <file-or-directory>...
#   Directories are signed recursively (every regular file except *.asc,
#   *.clamav and sourceInfo.*).
#
# Environment:
#   KEYNAME               GPG key id       (default 86FEC04D)
#   ARANGO_SIGN_PASSWD    key passphrase   (required)
#   GNUPGHOME             populated GnuPG home directory (default ~/.gnupg) —
#                         in CI a private copy of the .gnupg4 directory of the
#                         arangodb-helper/package-signing repo, the same
#                         directory Jenkins mounted as /root/.gnupg. It ships
#                         secring.gpg (no private-keys-v1.d), so the secret
#                         key is imported below before signing.

set -euo pipefail

KEYNAME="${KEYNAME:-86FEC04D}"

if [ -z "${ARANGO_SIGN_PASSWD:-}" ]; then
  echo "sign-packages: ARANGO_SIGN_PASSWD is not set" >&2
  exit 1
fi

export GNUPGHOME="${GNUPGHOME:-${HOME}/.gnupg}"
if ! [ -d "${GNUPGHOME}" ]; then
  echo "sign-packages: GNUPGHOME (${GNUPGHOME}) does not exist" >&2
  exit 1
fi
chmod 700 "${GNUPGHOME}"

PASSPHRASE_FILE="$(mktemp)"
trap 'rm -f "${PASSPHRASE_FILE}"' EXIT
chmod 600 "${PASSPHRASE_FILE}"
echo "${ARANGO_SIGN_PASSWD}" > "${PASSPHRASE_FILE}"

gpg_sign() {
  gpg2 \
    --homedir="${GNUPGHOME}" \
    --no-permission-warning \
    --armor \
    --detach-sign \
    --sign \
    --batch \
    --pinentry-mode=loopback \
    --digest-algo SHA512 \
    --passphrase-file="${PASSPHRASE_FILE}" \
    --yes \
    -u "${KEYNAME}" \
    -o "$2" \
    "$1"
}

# setupGpg: import the secret keyring and validate by signing a test file.
gpgconf --kill gpg-agent || true
if [ -f "${GNUPGHOME}/secring.gpg" ]; then
  gpg2 --homedir="${GNUPGHOME}" --no-permission-warning --batch --import "${GNUPGHOME}/secring.gpg"
fi

TEST_FILE="$(mktemp)"
echo "this is a test" > "${TEST_FILE}"
gpg_sign "${TEST_FILE}" "${TEST_FILE}.asc"
rm -f "${TEST_FILE}" "${TEST_FILE}.asc"
echo "testing signing was successful"

sign_one() {
  local file="$1"
  local sig="$1.asc"
  if [ -s "${sig}" ] && [ "${sig}" -nt "${file}" ]; then
    echo "using existing signature ${sig}"
  else
    echo "signing file ${file}"
    gpg_sign "${file}" "${sig}"
  fi
}

for target in "$@"; do
  if [ -d "${target}" ]; then
    while IFS= read -r file; do
      sign_one "${file}"
    done < <(find "${target}" -type f ! -name "*.asc" ! -name "*.clamav" ! -name "sourceInfo.*" | sort)
  elif [ -f "${target}" ]; then
    sign_one "${target}"
  else
    # e.g. packages/ absent because all package jobs were disabled
    echo "sign-packages: skipping missing target: ${target}"
  fi
done
