#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-1.3.1}"
PKG_NAME="avacli"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Error: This script must be run on macOS"
    exit 1
fi

ARCH="$(uname -m)"
echo "==> Building ${PKG_NAME} ${VERSION} for macOS ${ARCH}"

MISSING_DEPS=()
for dep in cmake curl pkg-config; do
    if ! brew list "$dep" &>/dev/null; then
        MISSING_DEPS+=("$dep")
    fi
done

if [[ ${#MISSING_DEPS[@]} -gt 0 ]]; then
    echo "==> Installing missing dependencies: ${MISSING_DEPS[*]}"
    brew install "${MISSING_DEPS[@]}"
fi

if command -v ninja &>/dev/null; then
    GENERATOR="Ninja"
else
    GENERATOR="Unix Makefiles"
fi

echo "==> Configuring with CMake (generator: ${GENERATOR})"
cmake -B build -G "${GENERATOR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$(brew --prefix curl)"

echo "==> Building"
cmake --build build --config Release

if [[ ! -f build/avacli ]]; then
    echo "Error: Build failed — binary not found at build/avacli"
    exit 1
fi

echo "==> Packaging"
mkdir -p dist

STAGING="$(mktemp -d)/${PKG_NAME}-${VERSION}-macos-${ARCH}"
mkdir -p "$STAGING"

cp build/avacli "$STAGING/avacli"
chmod 755 "$STAGING/avacli"

cat > "$STAGING/install.sh" << 'INSTALL_EOF'
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
chmod 755 "$STAGING/install.sh"

TARBALL="dist/${PKG_NAME}-${VERSION}-macos-${ARCH}.tar.gz"
tar -czf "$TARBALL" -C "$(dirname "$STAGING")" "$(basename "$STAGING")"
rm -rf "$(dirname "$STAGING")"

echo "==> Generating checksum"
shasum -a 256 "$TARBALL" > "${TARBALL}.sha256"

echo ""
echo "Built: $TARBALL"
echo "Size:  $(du -h "$TARBALL" | cut -f1)"
echo "SHA256: $(cut -d' ' -f1 "${TARBALL}.sha256")"
