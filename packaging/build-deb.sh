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
Recommends: python3 (>= 3.8), python3-venv, nodejs (>= 18), npm
Replaces: avalynnai (<< 2.0.0)
Conflicts: avalynnai (<< 2.0.0)
Provides: avalynnai
Maintainer: iBerry420
Homepage: https://github.com/iBerry420/acos
Description: avacli (open source) - Single-binary autonomous AI agent platform powered by xAI Grok.
 avacli provides a local web UI, embedded WebIDE, agent tools, cloud fleet management (Vultr), and more.
 The core binary has no runtime dependencies beyond libcurl + libssl; python3-venv, nodejs, and npm
 are recommended only for users who intend to host long-running Python or Node services (e.g. a
 Discord bot) through the built-in process supervisor.
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

# NOTE: do NOT use `pkill -f avacli` here. `-f` matches against the full
# command line of every process, which means it also matches apt/dpkg
# (whose argv contains "avacli_<ver>_amd64.deb") and this prerm script
# itself (`/var/lib/dpkg/info/avacli.prerm`). Running it SIGTERMs dpkg
# mid-unpack and leaves the package in a "very bad inconsistent state".
# Use `pkill -x` (exact process-name match) instead.

case "$1" in
    remove|deconfigure)
        if command -v systemctl >/dev/null 2>&1; then
            systemctl stop avacli.service 2>/dev/null || true
        fi
        pkill -x avacli 2>/dev/null || true
        ;;
    upgrade|failed-upgrade)
        # On upgrade the binary is replaced atomically; running instances
        # keep executing the old inode and users can restart at their
        # leisure. Intentionally do NOT kill here.
        :
        ;;
esac

exit 0
EOF
chmod 755 "$PKG_DIR/DEBIAN/prerm"

cat > "$PKG_DIR/DEBIAN/postrm" << 'EOF'
#!/bin/sh
set -e

case "$1" in
    purge)
        # Clean up per-user state only on explicit purge. User data lives
        # under $HOME/.avacli which we deliberately do not touch here.
        :
        ;;
esac

exit 0
EOF
chmod 755 "$PKG_DIR/DEBIAN/postrm"

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
