#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-1.0.0}"
ARCH="${2:-amd64}"
BINARY="${3:-build/avacli}"
PKG_NAME="avacli"
PKG_DIR="$(mktemp -d)"
OUTDIR="${4:-.}"

if [[ ! -f "$BINARY" ]]; then
    echo "Error: Binary not found at $BINARY"
    echo "Usage: $0 [VERSION] [ARCH] [BINARY_PATH] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/local/bin"
mkdir -p "$PKG_DIR/usr/share/doc/$PKG_NAME"
mkdir -p "$PKG_DIR/etc/avacli"

cp "$BINARY" "$PKG_DIR/usr/local/bin/avacli"
chmod 755 "$PKG_DIR/usr/local/bin/avacli"

cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: $PKG_NAME
Version: $VERSION
Section: devel
Priority: optional
Architecture: $ARCH
Depends: libcurl4 (>= 7.68.0), libssl3 | libssl1.1
Maintainer: iBerry420
Homepage: https://github.com/iBerry420/acos
Description: avacli (open source) - Single-binary autonomous AI agent platform powered by xAI Grok.
 avacli provides a local web UI, embedded WebIDE, agent tools, cloud fleet management (Vultr), and more. No external runtimes required beyond libcurl.
EOF

cat > "$PKG_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/sh
set -e
echo "avacli (open source) installed successfully."
echo ""
echo "Quick start:"
echo "  avacli serve                  # Start web UI + backend on http://localhost:8080"
echo "  avacli chat \"Show me the logs\"   # One-shot CLI chat with agent"
echo ""
echo "Build from source or use the .deb for easy install."
echo "Documentation: https://github.com/iBerry420/acos"
EOF
chmod 755 "$PKG_DIR/DEBIAN/postinst"

cat > "$PKG_DIR/DEBIAN/prerm" << 'EOF'
#!/bin/sh
set -e
# Stop any running avacli instances
pkill -f "avacli" 2>/dev/null || true
EOF
chmod 755 "$PKG_DIR/DEBIAN/prerm"

cat > "$PKG_DIR/usr/share/doc/$PKG_NAME/copyright" << EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: avacli
Source: https://github.com/iBerry420/acos

Files: *
Copyright: 2024-2025 iBerry420 / avalynn-ai
License: MIT
 See the LICENSE file in the repository root for full terms.
EOF

cat > "$PKG_DIR/etc/avacli/config.json" << EOF
{
    "platform_url": "https://github.com/iBerry420/acos",
    "default_model": "grok-beta",
    "serve_port": 8080,
    "serve_host": "0.0.0.0"
}
EOF

cat > "$PKG_DIR/DEBIAN/conffiles" << EOF
/etc/avacli/config.json
EOF

DEB_FILE="$OUTDIR/${PKG_NAME}_${VERSION}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$PKG_DIR" "$DEB_FILE"
rm -rf "$PKG_DIR"

echo "Built: $DEB_FILE"
echo "Size: $(du -h "$DEB_FILE" | cut -f1)"
echo "Package ready for avacli (open source) distribution."
