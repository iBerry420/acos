#!/usr/bin/env bash
# Build avacli, publish .deb to the APT pool, refresh dist/ tarballs + checksums, re-sign Release.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
VERSION="${1:?usage: $0 <version> [REPO_ROOT]}"
REPO_ROOT="${2:-/var/www/packages.avalynn.ai}"

cd "$PROJECT_DIR"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc)"

BINARY="$PROJECT_DIR/build/avacli"
if [[ ! -f "$BINARY" ]]; then
    echo "Build failed: missing $BINARY"
    exit 1
fi

DIST="$PROJECT_DIR/dist"
mkdir -p "$DIST"
ARCH=$(dpkg --print-architecture 2>/dev/null || echo "amd64")
UARCH=$(uname -m)

bash "$SCRIPT_DIR/build-deb.sh" "$VERSION" "$ARCH" "$BINARY" "$DIST"
bash "$SCRIPT_DIR/build-tarball.sh" "$VERSION" "$UARCH" "$BINARY" "$DIST"

cd "$DIST"
sha256sum ./*.deb ./*.tar.gz 2>/dev/null > SHA256SUMS || true

install -m0644 "$DIST/avacli_${VERSION}_${ARCH}.deb" "$REPO_ROOT/pool/main/"
shopt -s nullglob
for f in "$DIST"/avacli-${VERSION}-linux-*.tar.gz "$DIST"/avacli-${VERSION}-linux-*.tar.gz.sha256; do
    [[ -f "$f" ]] && install -m0644 "$f" "$REPO_ROOT/dist/"
done
shopt -u nullglob
cp -f "$DIST/SHA256SUMS" "$REPO_ROOT/dist/SHA256SUMS" 2>/dev/null || true

# Stable download aliases (x86_64 / aarch64 naming matches download page)
cd "$REPO_ROOT/dist"
case "$UARCH" in
    x86_64)
        ln -sf "avacli-${VERSION}-linux-x86_64.tar.gz" avacli-latest-linux-x86_64.tar.gz
        ;;
    aarch64|arm64)
        ln -sf "avacli-${VERSION}-linux-${UARCH}.tar.gz" avacli-latest-linux-aarch64.tar.gz
        ;;
esac

bash "$SCRIPT_DIR/publish-repo.sh" "$REPO_ROOT"

echo "Done. Published avacli ${VERSION} to ${REPO_ROOT}"
