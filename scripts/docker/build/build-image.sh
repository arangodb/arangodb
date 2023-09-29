set -e

case "$1" in
  amd64)
    echo "Building amd64 image"
    ;;
  arm64)
    echo "Building arm64 image"
    ;;
  *)
    echo "Unknown architecture '$1'"
    exit 15
    ;;
esac
