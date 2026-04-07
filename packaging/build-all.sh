#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
VERSION="${1:-1.0.0}"
OUTDIR="${2:-$PROJECT_DIR/dist}"

echo "=== avacli (open source) build pipeline v${VERSION} ==="
echo "Output: $OUTDIR"
echo ""

mkdir -p "$OUTDIR"

echo "[1/4] Building binary..."
cd "$PROJECT_DIR"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd "$PROJECT_DIR"

BINARY="build/avacli"
if [[ ! -f "$BINARY" ]]; then
    echo "Error: Build failed - binary not found"
    exit 1
fi

ARCH=$(dpkg --print-architecture 2>/dev/null || echo "amd64")
UARCH=$(uname -m)

echo "[2/4] Building .deb package..."
bash "$SCRIPT_DIR/build-deb.sh" "$VERSION" "$ARCH" "$BINARY" "$OUTDIR"

echo "[3/4] Building tarball..."
bash "$SCRIPT_DIR/build-tarball.sh" "$VERSION" "$UARCH" "$BINARY" "$OUTDIR"

echo "[4/4] Generating checksums..."
cd "$OUTDIR"
sha256sum *.deb *.tar.gz > SHA256SUMS 2>/dev/null || true

echo ""
echo "=== Build complete ==="
ls -lh "$OUTDIR"/*
echo ""
echo "To publish to APT repo or GitHub Releases:"
echo "  See packaging/publish-repo.sh and GitHub workflow in .github/"
