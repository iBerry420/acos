#!/usr/bin/env bash
# install.sh - Install avacli on a remote server
# Usage: ./install.sh [--api-key YOUR_KEY]
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$SCRIPT_DIR/avacli"
INSTALL_DIR="/usr/local/bin"
CONFIG_DIR="$HOME/.avacli"
USE_SUDO=""

echo "=== avacli installer ==="
echo ""

# --- Preflight checks ---

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "Error: This installer only supports Linux."
    exit 1
fi

ARCH="$(uname -m)"
if [[ "$ARCH" != "x86_64" ]]; then
    echo "Error: This binary is built for x86_64, but this system is $ARCH."
    exit 1
fi

if [[ ! -f "$BINARY" ]]; then
    echo "Error: avacli binary not found at $BINARY"
    echo "Place the binary in the same directory as this script."
    exit 1
fi

if [[ ! -x "$BINARY" ]]; then
    chmod +x "$BINARY"
fi

# --- Install runtime dependency (libcurl) ---

install_libcurl() {
    if ldconfig -p 2>/dev/null | grep -q libcurl.so.4; then
        echo "[ok] libcurl4 is installed"
        return
    fi

    echo "Installing libcurl4 (required runtime dependency)..."

    if command -v apt-get &>/dev/null; then
        if command -v sudo &>/dev/null; then
            sudo apt-get update -qq && sudo apt-get install -y libcurl4
        else
            echo "Warning: sudo not available. Install libcurl4 manually:"
            echo "  apt-get install libcurl4"
            echo ""
        fi
    elif command -v dnf &>/dev/null; then
        if command -v sudo &>/dev/null; then
            sudo dnf install -y libcurl
        else
            echo "Warning: sudo not available. Install libcurl manually:"
            echo "  dnf install libcurl"
            echo ""
        fi
    elif command -v yum &>/dev/null; then
        if command -v sudo &>/dev/null; then
            sudo yum install -y libcurl
        else
            echo "Warning: sudo not available. Install libcurl manually."
            echo ""
        fi
    else
        echo "Warning: Could not detect package manager."
        echo "Please install libcurl4 manually before running avacli."
        echo ""
    fi
}

install_libcurl

# --- Install binary ---

if command -v sudo &>/dev/null && sudo -n true 2>/dev/null; then
    USE_SUDO="sudo"
elif command -v sudo &>/dev/null; then
    echo ""
    echo "Installing to $INSTALL_DIR (requires sudo)..."
    USE_SUDO="sudo"
else
    INSTALL_DIR="$HOME/.local/bin"
    echo "No sudo available. Installing to $INSTALL_DIR instead."
fi

mkdir -p "$INSTALL_DIR" 2>/dev/null || $USE_SUDO mkdir -p "$INSTALL_DIR"

echo "Copying avacli to $INSTALL_DIR/"
$USE_SUDO cp "$BINARY" "$INSTALL_DIR/avacli"
$USE_SUDO chmod +x "$INSTALL_DIR/avacli"

# Ensure ~/.local/bin is in PATH if we used it
if [[ "$INSTALL_DIR" == "$HOME/.local/bin" ]]; then
    if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
        echo ""
        echo "Adding ~/.local/bin to PATH in ~/.bashrc"
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
        export PATH="$HOME/.local/bin:$PATH"
    fi
fi

# --- Config directory ---

mkdir -p "$CONFIG_DIR"

# --- API key setup ---

parse_api_key() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --api-key) echo "$2"; return ;;
            --api-key=*) echo "${1#*=}"; return ;;
        esac
        shift
    done
}

API_KEY="$(parse_api_key "$@")"

if [[ -z "$API_KEY" ]] && [[ -z "${XAI_API_KEY:-}" ]]; then
    if [[ -f "$CONFIG_DIR/config.json" ]]; then
        echo "[ok] API key already configured in $CONFIG_DIR/config.json"
    elif [[ -t 0 ]]; then
        echo ""
        read -r -p "Enter your xAI API key (or press Enter to skip): " API_KEY
    fi
fi

if [[ -n "$API_KEY" ]]; then
    "$INSTALL_DIR/avacli" --set-api-key "$API_KEY"
fi

# --- Verify ---

echo ""
if "$INSTALL_DIR/avacli" --help &>/dev/null; then
    echo "[ok] avacli installed successfully"
else
    echo "Warning: avacli --help failed. Check for missing libraries:"
    echo "  ldd $INSTALL_DIR/avacli"
    exit 1
fi

# --- Summary ---

echo ""
echo "=== Installation complete ==="
echo ""
echo "  Binary:  $INSTALL_DIR/avacli"
echo "  Config:  $CONFIG_DIR/"
echo ""
echo "Quick start:"
echo "  avacli                              # Interactive TUI (question mode)"
echo "  avacli --mode agent -d /path/to/project  # Agent mode on a project"
echo "  avacli --non-interactive -t \"task\"   # Headless / scripting"
echo "  avacli --list-models                # Show available models"
echo "  avacli --help                       # Full options"
echo ""
