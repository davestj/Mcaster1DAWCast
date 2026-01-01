#!/usr/bin/env bash
# build-rpm.sh — Build RPM package for Mcaster1DAWCast
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/../.."
VERSION=$(cat "$PROJECT_ROOT/VERSION.txt" | tr -d '[:space:]')

echo "── Mcaster1DAWCast — RPM Package Build (v${VERSION}) ──"

# Create RPM build tree
mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Create source tarball
cd "$PROJECT_ROOT/.."
tar czf ~/rpmbuild/SOURCES/mcaster1dawcast-${VERSION}.tar.gz \
    --transform="s|^Mcaster1DAWCast|mcaster1dawcast-${VERSION}|" \
    Mcaster1DAWCast/

# Copy spec file
cp "$SCRIPT_DIR/mcaster1dawcast.spec" ~/rpmbuild/SPECS/

# Build RPM
rpmbuild -bb ~/rpmbuild/SPECS/mcaster1dawcast.spec

echo ""
echo "── Done. RPM package: ──"
ls -la ~/rpmbuild/RPMS/*/*.rpm 2>/dev/null || echo "No RPM found — check build output."
