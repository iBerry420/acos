#!/usr/bin/env bash
# build.sh - Build avacli
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "=== avacli build script ==="

# Detect OS and install deps (Ubuntu/Debian)
if command -v apt-get &>/dev/null; then
    echo "Checking system dependencies..."
    MISSING=""
    for pkg in build-essential cmake libcurl4-openssl-dev libssl-dev; do
        if ! dpkg -s "$pkg" &>/dev/null 2>&1; then
            MISSING="$MISSING $pkg"
        fi
    done
    if [[ -n "$MISSING" ]]; then
        echo "Installing:$MISSING (may need sudo)..."
        sudo apt-get update -qq
        sudo apt-get install -y $MISSING
    fi
fi

# Create build dir
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "Running CMake..."
cmake ..

echo ""
echo "Building..."
make -j"$(nproc 2>/dev/null || echo 4)"

echo ""
echo "=== Build complete ==="
echo "Binary: ${BUILD_DIR}/avacli"
echo ""

# Offer to install (skip if non-interactive or --no-install)
if [[ -t 0 ]] && [[ "${1:-}" != "--no-install" ]]; then
    read -r -p "Install to /usr/local/bin? [y/N] " reply 2>/dev/null || true
    if [[ "$reply" =~ ^[Yy]$ ]]; then
        sudo cp "${BUILD_DIR}/avacli" /usr/local/bin/
        echo "Installed to /usr/local/bin/avacli"
    fi
fi
