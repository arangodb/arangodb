#!/bin/bash
# Downloads a release if it exists or builds from source if it doesn't
#
#   provision_operator_sidecar.sh [--dest PATH] [--arch x64|aarch64] [--ref REF]
#                                 [--repo URL] [--release-tag TAG] [--force]
#
set -u -o pipefail

DEST=""
ARCH=""
REF="${KUBE_ARANGODB_REF:-master}"
REPO="${KUBE_ARANGODB_REPO:-https://github.com/arangodb/kube-arangodb.git}"
RELEASE_TAG="latest"
FORCE=0
GITHUB_API="https://api.github.com/repos/arangodb/kube-arangodb"

while [ $# -gt 0 ]; do
    case "$1" in
        --dest)         DEST="${2:-}"; shift 2 ;;
        --arch)         ARCH="${2:-}"; shift 2 ;;
        --ref)          REF="${2:-}"; shift 2 ;;
        --repo)         REPO="${2:-}"; shift 2 ;;
        --release-tag)  RELEASE_TAG="${2:-}"; shift 2 ;;
        --force)        FORCE=1; shift ;;
        -h|--help)      sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 64 ;;
    esac
done

# Everything informational goes to stderr; stdout carries only the path.
log() { echo "$*" >&2; }
die() { echo "error: $*" >&2; exit 1; }

# --- architecture ----------------------------------------------------------
# CI speaks x64/aarch64, Go speaks amd64/arm64.
if [ -z "$ARCH" ]; then
    ARCH="$(uname -m)"
fi
case "$ARCH" in
    x64|x86_64|amd64)   GOARCH="amd64" ;;
    aarch64|arm64)      GOARCH="arm64" ;;
    *) die "unsupported architecture: $ARCH" ;;
esac

if [ -z "$DEST" ]; then
    DEST="${RBAC_OPERATOR_DIR:-/tmp/rbac-operator}/arangodb_operator"
fi
mkdir -p "$(dirname "$DEST")" || die "cannot create $(dirname "$DEST")"
DEST="$(cd "$(dirname "$DEST")" && pwd)/$(basename "$DEST")"

usable() {
    [ -x "$1" ] || return 1
    "$1" sidecar --help >/dev/null 2>&1
}

report_and_exit() {
    echo "$1"
    exit 0
}

if [ "$FORCE" -eq 0 ] && usable "$DEST"; then
    log "reusing existing operator binary: $DEST"
    report_and_exit "$DEST"
fi

# --- attempt 1: download a release asset -----------------------------------
ASSET="arangodb_operator_linux_${GOARCH}"
log "looking for release asset $ASSET ($RELEASE_TAG)..."

if [ "$RELEASE_TAG" = "latest" ]; then
    RELEASE_URL="$GITHUB_API/releases/latest"
else
    RELEASE_URL="$GITHUB_API/releases/tags/$RELEASE_TAG"
fi

AUTH_HEADER=()
if [ -n "${GITHUB_TOKEN:-}" ]; then
    AUTH_HEADER=(-H "Authorization: Bearer $GITHUB_TOKEN")
fi

RELEASE_JSON="$(curl -sS -m 60 "${AUTH_HEADER[@]+"${AUTH_HEADER[@]}"}" "$RELEASE_URL" 2>/dev/null || true)"
DOWNLOAD_URL=""
if [ -n "$RELEASE_JSON" ]; then
    # Resolve the asset by name from the API rather than guessing a URL, so a
    # renamed or absent asset is detected instead of downloading an error page.
    DOWNLOAD_URL="$(printf '%s' "$RELEASE_JSON" | python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(0)
for asset in data.get('assets') or []:
    if asset.get('name') == '$ASSET':
        print(asset.get('browser_download_url', ''))
        break
" 2>/dev/null || true)"
fi

if [ -n "$DOWNLOAD_URL" ]; then
    log "downloading $DOWNLOAD_URL"
    if curl -sS -L -m 600 -o "$DEST.part" "$DOWNLOAD_URL" && chmod +x "$DEST.part"; then
        mv -f "$DEST.part" "$DEST"
        if usable "$DEST"; then
            log "downloaded operator binary: $DEST"
            report_and_exit "$DEST"
        fi
        log "the downloaded asset does not run as \`arangodb_operator sidecar\`; building instead"
    else
        log "download failed; building instead"
    fi
    rm -f "$DEST.part"
else
    # Expected today. Not a warning.
    log "no $ASSET asset published for this release; building from source"
fi

# Fetch the source as a tarball
SRC="${KUBE_ARANGODB_SRC:-}"
if [ -n "$SRC" ] && [ -d "$SRC/cmd/main" ]; then
    log "building from the existing checkout at $SRC"
else
    SRC="${RBAC_OPERATOR_DIR:-/tmp/rbac-operator}/kube-arangodb"
    SLUG="$(printf '%s' "$REPO" | sed -nE 's#^(https://|git@)github\.com[:/](.+)$#\2#p')"
    SLUG="${SLUG%/}"; SLUG="${SLUG%.git}"
    if [ -n "$SLUG" ]; then
        log "fetching $SLUG at $REF as a tarball"
        rm -rf "$SRC"
        mkdir -p "$SRC"
        curl -sS -L -m 600 "https://codeload.github.com/$SLUG/tar.gz/$REF" \
            | tar -C "$SRC" -xz --strip-components=1 \
            || die "could not fetch $SLUG at ref '$REF'.
 Check the ref exists - note kube-arangodb's default branch is \`master\`, not \`main\`."
    else
        die "cannot fetch $REPO: it is not a GitHub URL"
    fi
fi
[ -d "$SRC/cmd/main" ] || die "$SRC does not look like kube-arangodb (no cmd/main)"

NEEDED_GO="$(sed -nE 's/^go ([0-9]+\.[0-9]+).*/\1/p' "$SRC/go.mod" | head -1)"
log "kube-arangodb go.mod requires go $NEEDED_GO"

GO_BIN="$(command -v go || true)"
if [ -n "$GO_BIN" ]; then
    HAVE_GO="$("$GO_BIN" env GOVERSION 2>/dev/null | sed -nE 's/^go([0-9]+\.[0-9]+).*/\1/p')"
    log "found go $HAVE_GO at $GO_BIN"
    # 1.21 is where automatic toolchain management arrived.
    if [ -n "$HAVE_GO" ] && [ "$(printf '%s\n1.21\n' "$HAVE_GO" | sort -V | head -1)" != "1.21" ]; then
        log "go $HAVE_GO is too old to fetch a toolchain itself; bootstrapping a newer one"
        GO_BIN=""
    fi
fi

if [ -z "$GO_BIN" ]; then
    BOOTSTRAP_VER="${GO_BOOTSTRAP_VERSION:-1.25.1}"
    GOROOT_DIR="${RBAC_OPERATOR_DIR:-/tmp/rbac-operator}/go"
    if [ ! -x "$GOROOT_DIR/bin/go" ]; then
        TARBALL="go${BOOTSTRAP_VER}.linux-${GOARCH}.tar.gz"
        log "downloading $TARBALL"
        rm -rf "$GOROOT_DIR"
        mkdir -p "$(dirname "$GOROOT_DIR")"
        curl -sS -L -m 600 -o "/tmp/$TARBALL" "https://go.dev/dl/$TARBALL" \
            || die "could not download the Go toolchain ($TARBALL).
 No usable Go is present and it cannot be fetched. Either run this on an image
 with Go >= 1.21, or pre-populate $GOROOT_DIR."
        mkdir -p "$GOROOT_DIR"
        tar -C "$(dirname "$GOROOT_DIR")" -xzf "/tmp/$TARBALL" || die "could not unpack $TARBALL"
        rm -f "/tmp/$TARBALL"
    fi
    export GOROOT="$GOROOT_DIR"
    GO_BIN="$GOROOT_DIR/bin/go"
    export PATH="$GOROOT_DIR/bin:$PATH"
fi

log "building arangodb_operator for linux/$GOARCH (this takes a few minutes)"
(
    cd "$SRC" || exit 1
    # Mirrors kube-arangodb's own binary_operator rule.
    CGO_ENABLED=0 GOOS=linux GOARCH="$GOARCH" GOTOOLCHAIN=auto \
        "$GO_BIN" build -o "$DEST" ./cmd/main
) >&2 || die "building the operator failed. Output above; the usual causes are a
 Go version mismatch or no network access for module downloads."

usable "$DEST" || die "the freshly built $DEST does not run as \`arangodb_operator sidecar\`"
log "built operator binary: $DEST"
report_and_exit "$DEST"
