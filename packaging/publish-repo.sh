#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="${1:-/var/www/packages.avalynn.ai}"
GPG_KEY_EMAIL="packages@avalynn.ai"
CODENAME="stable"
COMPONENT="main"

echo "=== Publishing APT Repository ==="

cd "$REPO_ROOT"

for ARCH in amd64 arm64; do
    PACKAGES_DIR="dists/$CODENAME/$COMPONENT/binary-$ARCH"
    mkdir -p "$PACKAGES_DIR"

    # Filter complete package stanzas by architecture with blank line separators
    apt-ftparchive packages "pool/$COMPONENT/" | awk -v arch="$ARCH" '
        /^$/ { if (buf != "" && found) { if (n++) print ""; print buf } buf=""; found=0; next }
        { buf = (buf == "" ? $0 : buf "\n" $0) }
        /^Architecture: / && $2 == arch { found=1 }
        END { if (buf != "" && found) { if (n++) print ""; print buf } }
    ' > "$PACKAGES_DIR/Packages"

    gzip -9c "$PACKAGES_DIR/Packages" > "$PACKAGES_DIR/Packages.gz"
    echo "Generated $PACKAGES_DIR/Packages"
done

rm -f "dists/$CODENAME/Release" "dists/$CODENAME/Release.gpg" "dists/$CODENAME/InRelease"
apt-ftparchive -c apt-repo.conf release "dists/$CODENAME" > "dists/$CODENAME/Release.tmp"
mv "dists/$CODENAME/Release.tmp" "dists/$CODENAME/Release"
gpg --batch --yes --pinentry-mode loopback --default-key "$GPG_KEY_EMAIL" -abs -o "dists/$CODENAME/Release.gpg" "dists/$CODENAME/Release"
gpg --batch --yes --pinentry-mode loopback --default-key "$GPG_KEY_EMAIL" --clearsign -o "dists/$CODENAME/InRelease" "dists/$CODENAME/Release"

echo ""
echo "=== Repository published ==="
echo ""
echo "Client install command:"
echo "  curl -fsSL https://packages.avalynn.ai/key.gpg | sudo gpg --dearmor -o /etc/apt/trusted.gpg.d/avalynn.gpg"
echo "  echo 'deb [arch=amd64] https://packages.avalynn.ai $CODENAME $COMPONENT' | sudo tee /etc/apt/sources.list.d/avalynn.list"
echo "  sudo apt update && sudo apt install avacli"
