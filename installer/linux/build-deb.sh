#!/usr/bin/env bash
# build-deb.sh — Build Debian/Ubuntu .deb package for Mcaster1DAWCast
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/../.."

echo "── Mcaster1DAWCast — Debian Package Build ──"

cd "$PROJECT_ROOT"

# Ensure source is buildable
if [ ! -f configure ]; then
    echo "Running autogen.sh..."
    ./autogen.sh
fi

# Build the .deb
dpkg-buildpackage -b -us -uc

echo ""
echo "── Done. .deb package is in parent directory ──"
ls -la ../*.deb 2>/dev/null || echo "No .deb found — check build output above."
