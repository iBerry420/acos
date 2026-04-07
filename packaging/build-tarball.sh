#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-1.0.0}"
ARCH="${2:-x86_64}"
BINARY="${3:-build/avacli}"
PKG_NAME="avacli"
OUTDIR="${4:-.}"

if [[ ! -f "$BINARY" ]]; then
    echo "Error: Binary not found at $BINARY"
    exit 1
fi

case "$(uname -s)" in
    Darwin) PLATFORM="macos" ;;
    *)      PLATFORM="linux" ;;
esac

TARDIR="$(mktemp -d)/${PKG_NAME}-${VERSION}-${PLATFORM}-${ARCH}"
mkdir -p "$TARDIR"

cp "$BINARY" "$TARDIR/avacli"
chmod 755 "$TARDIR/avacli"

cat > "$TARDIR/install.sh" << 'INSTALL_EOF'
#!/usr/bin/env bash
set -euo pipefail

INSTALL_DIR="${1:-/usr/local/bin}"

if [[ $EUID -ne 0 ]] && [[ "$INSTALL_DIR" == /usr* ]]; then
    echo "Root required to install to $INSTALL_DIR. Re-running with sudo..."
    exec sudo "$0" "$@"
fi

cp avacli "$INSTALL_DIR/avacli"
chmod 755 "$INSTALL_DIR/avacli"

echo "Avacli installed to $INSTALL_DIR/avacli"
echo ""
echo "Quick start:"
echo "  avacli --set-platform-key YOUR_KEY   # Connect to platform"
echo "  avacli                               # Start web UI (default)"
echo "  avacli chat \"hello\"                  # One-shot CLI chat"
INSTALL_EOF
chmod 755 "$TARDIR/install.sh"

cat > "$TARDIR/README.txt" << 'README_EOF'
Avacli - Autonomous AI Agent for xAI Grok Models
====================================================

Quick Install:
  sudo ./install.sh

Manual Install:
  sudo cp avacli /usr/local/bin/
  sudo chmod 755 /usr/local/bin/avacli

Usage:
  avacli --set-platform-key KEY  Connect to Avacli platform
  avacli                         Start web UI (default: http://localhost:8080)
  avacli chat "message"          Send a one-shot chat message
  avacli --local-mode     Use direct xAI API (bypass platform)
  avacli --help           Show all options

Documentation: https://avalynn.ai/platform/download.php
README_EOF

TARBALL="$OUTDIR/${PKG_NAME}-${VERSION}-${PLATFORM}-${ARCH}.tar.gz"
tar -czf "$TARBALL" -C "$(dirname "$TARDIR")" "$(basename "$TARDIR")"
rm -rf "$(dirname "$TARDIR")"

echo "Built: $TARBALL"
echo "Size: $(du -h "$TARBALL" | cut -f1)"

if [[ "$PLATFORM" == "macos" ]]; then
    shasum -a 256 "$TARBALL" > "${TARBALL}.sha256"
else
    sha256sum "$TARBALL" > "${TARBALL}.sha256"
fi
echo "Checksum: $(cat "${TARBALL}.sha256")"
