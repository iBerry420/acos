#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="${1:-/var/www/packages.avalynn.ai}"
GPG_KEY_EMAIL="packages@avalynn.ai"
GPG_KEY_NAME="Avacli Package Signing Key"
CODENAME="stable"
COMPONENT="main"

echo "=== Avacli APT Repository Setup ==="
echo "Repo root: $REPO_ROOT"
echo ""

mkdir -p "$REPO_ROOT/pool/$COMPONENT"
mkdir -p "$REPO_ROOT/dists/$CODENAME/$COMPONENT/binary-amd64"
mkdir -p "$REPO_ROOT/dists/$CODENAME/$COMPONENT/binary-arm64"

if ! gpg --list-keys "$GPG_KEY_EMAIL" &>/dev/null; then
    echo "Generating GPG signing key..."
    cat > /tmp/gpg-keygen-params << EOF
%no-protection
Key-Type: RSA
Key-Length: 4096
Subkey-Type: RSA
Subkey-Length: 4096
Name-Real: $GPG_KEY_NAME
Name-Email: $GPG_KEY_EMAIL
Expire-Date: 0
%commit
EOF
    gpg --batch --gen-key /tmp/gpg-keygen-params
    rm -f /tmp/gpg-keygen-params
    echo "GPG key generated for $GPG_KEY_EMAIL"
else
    echo "GPG key already exists for $GPG_KEY_EMAIL"
fi

gpg --armor --export "$GPG_KEY_EMAIL" > "$REPO_ROOT/key.gpg"
gpg --export "$GPG_KEY_EMAIL" > "$REPO_ROOT/key.gpg.bin"
echo "Public key exported to $REPO_ROOT/key.gpg"

cat > "$REPO_ROOT/apt-repo.conf" << EOF
APT::FTPArchive::Release::Origin "Avacli";
APT::FTPArchive::Release::Label "Avacli";
APT::FTPArchive::Release::Suite "$CODENAME";
APT::FTPArchive::Release::Codename "$CODENAME";
APT::FTPArchive::Release::Architectures "amd64 arm64";
APT::FTPArchive::Release::Components "$COMPONENT";
APT::FTPArchive::Release::Description "Avacli Package Repository";
EOF

echo ""
echo "=== Repository structure created ==="
echo ""
echo "Next steps:"
echo "  1. Copy .deb files to: $REPO_ROOT/pool/$COMPONENT/"
echo "  2. Run: $(dirname "$0")/publish-repo.sh $REPO_ROOT"
echo "  3. Configure web server to serve $REPO_ROOT at packages.avalynn.ai"
echo ""
echo "Nginx config example:"
echo "  server {"
echo "      listen 443 ssl;"
echo "      server_name packages.avalynn.ai;"
echo "      root $REPO_ROOT;"
echo "      autoindex on;"
echo "  }"
