#!/bin/bash
# ──────────────────────────────────────────────────────────────────────────────
# build-pkg.sh — Mcaster1DAWCast macOS PKG Installer Builder
#
# Installs to:  /Applications/Mcaster1DAWCast.app
# Post-install: Copies to ~/Mcaster1/Mcaster1DAWCast/ for portable layout
# Output:       installer/macos/Mcaster1DAWCast-<version>-macOS-arm64.pkg
#
# Signing:
#   App bundle:  Developer ID Application (hardened runtime)
#   PKG:         Developer ID Installer (by SHA1 hash)
# Notarize:      xcrun notarytool with "Mcaster1" keychain profile
#
# Usage:
#   bash installer/macos/build-pkg.sh
#
# Prerequisites:
#   - Release build exists at build-release/bin/RELEASE/Mcaster1DAWCast.app
#   - Developer ID Application + Installer certs in keychain
#   - "Mcaster1" notarytool keychain profile stored
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-release"
APP_NAME="Mcaster1DAWCast"
IDENTIFIER="com.mcaster1.Mcaster1DAWCast"
OUTPUT_DIR="$SCRIPT_DIR"

# Read version from VERSION.txt
VERSION_FILE="$PROJECT_DIR/VERSION.txt"
if [ -f "$VERSION_FILE" ]; then
    VERSION_FULL="$(tr -d '[:space:]' < "$VERSION_FILE")"
else
    echo "WARNING: VERSION.txt not found at $VERSION_FILE — defaulting to 1.0.0-alpha"
    VERSION_FULL="1.0.0-alpha"
fi
# Extract numeric version (strip pre-release suffix)
VERSION="$(echo "$VERSION_FULL" | sed 's/-.*//')"

PKG_OUTPUT="$OUTPUT_DIR/${APP_NAME}-${VERSION_FULL}-macOS-arm64.pkg"
ATTESTATION_OUTPUT="$OUTPUT_DIR/${APP_NAME}-${VERSION_FULL}-macOS-arm64.pkg.attestation.txt"

# Signing identities (by SHA1 hash to avoid ambiguity)
APP_SIGN_HASH="040C503B906961B9D848F989D778ED754D3C2770"
APP_SIGN_ID="Developer ID Application: David St John (FCA38UPLY3)"
PKG_SIGN_HASH="B6267A54A4408A276B5B8DFA77C29D15EA8E7668"
PKG_SIGN_ID="Developer ID Installer: David St John (FCA38UPLY3)"
TEAM_ID="FCA38UPLY3"

# Source paths
APP_BUNDLE="$BUILD_DIR/bin/RELEASE/${APP_NAME}.app"
ENTITLEMENTS="$SCRIPT_DIR/entitlements.plist"

# Working dirs
PKG_ROOT="$BUILD_DIR/pkg-root"
PKG_SCRIPTS="$BUILD_DIR/pkg-scripts"
PKG_RESOURCES="$BUILD_DIR/pkg-resources"
COMPONENT_PKG="$BUILD_DIR/${APP_NAME}-component.pkg"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  Mcaster1DAWCast macOS PKG Builder                         ║"
echo "║  Version:  $VERSION_FULL                                   ║"
echo "║  Arch:     arm64 (Apple Silicon)                           ║"
echo "║  Install:  /Applications + ~/Mcaster1/Mcaster1DAWCast/     ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ── Step 1: Verify build ─────────────────────────────────────────────────────
if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: App bundle not found at: $APP_BUNDLE"
    echo "Build first:"
    echo "  cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \\"
    echo "    -DBUILD_TESTS=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt"
    echo "  cmake --build build-release"
    exit 1
fi
echo "[1/10] Build verified: $APP_BUNDLE"

# ── Step 2: Run macdeployqt ──────────────────────────────────────────────────
echo "[2/10] Running macdeployqt..."
MACDEPLOYQT=$(command -v macdeployqt || echo "/opt/homebrew/bin/macdeployqt")
"$MACDEPLOYQT" "$APP_BUNDLE" -always-overwrite 2>&1 | tail -5 || true
echo "      macdeployqt complete."

# ── Step 3: Copy resources into app bundle ───────────────────────────────────
echo "[3/10] Copying resources..."
RESOURCES="$APP_BUNDLE/Contents/Resources"

# Portable app directory structure
mkdir -p "$RESOURCES/themes" "$RESOURCES/docs" \
         "$RESOURCES/configs/dsp_presets" "$RESOURCES/configs/export_profiles" \
         "$RESOURCES/config/Mcaster1" "$RESOURCES/data" "$RESOURCES/logs"

# Themes (full theme directories)
for theme_dir in "$PROJECT_DIR/themes/"*/; do
    theme_name="$(basename "$theme_dir")"
    mkdir -p "$RESOURCES/themes/$theme_name"
    cp -Rf "$theme_dir"* "$RESOURCES/themes/$theme_name/" 2>/dev/null || true
done

# DSP presets
shopt -s nullglob
for preset in "$PROJECT_DIR/configs/dsp_presets/"*.yaml; do
    cp -f "$preset" "$RESOURCES/configs/dsp_presets/"
done

# Export profiles
for profile in "$PROJECT_DIR/configs/export_profiles/"*.yaml; do
    cp -f "$profile" "$RESOURCES/configs/export_profiles/"
done
shopt -u nullglob

# Default project config
[ -f "$PROJECT_DIR/configs/default_project.json" ] && \
    cp -f "$PROJECT_DIR/configs/default_project.json" "$RESOURCES/configs/"

# Documentation
for doc in "$PROJECT_DIR/docs/"*.html "$PROJECT_DIR/docs/"*.md; do
    [ -f "$doc" ] && cp -f "$doc" "$RESOURCES/docs/"
done
[ -f "$PROJECT_DIR/README.md" ] && cp -f "$PROJECT_DIR/README.md" "$RESOURCES/docs/"

# App icon
[ -f "$PROJECT_DIR/image_resources/Mcaster1DAWCast.icns" ] && \
    cp -f "$PROJECT_DIR/image_resources/Mcaster1DAWCast.icns" "$RESOURCES/"

echo "      Resources copied."

# ── Step 4: Code sign app bundle (inside-out, hardened runtime) ──────────────
echo "[4/10] Code signing app bundle (inside-out, hardened runtime + timestamp)..."

# Pass 1: Frameworks — individual dylibs
find "$APP_BUNDLE/Contents/Frameworks" -name "*.dylib" -type f 2>/dev/null | while read -r lib; do
    codesign --force --sign "$APP_SIGN_HASH" --options runtime --timestamp "$lib"
done

# Pass 2: Framework bundles
for fw in "$APP_BUNDLE/Contents/Frameworks/"*.framework; do
    [ -d "$fw" ] && codesign --force --deep --sign "$APP_SIGN_HASH" --options runtime --timestamp "$fw"
done

# Pass 3: Qt plugins
find "$APP_BUNDLE/Contents/PlugIns" -name "*.dylib" -type f 2>/dev/null | while read -r lib; do
    codesign --force --sign "$APP_SIGN_HASH" --options runtime --timestamp "$lib"
done

# Pass 4: Main executable
codesign --force --sign "$APP_SIGN_HASH" --options runtime --timestamp \
    "$APP_BUNDLE/Contents/MacOS/Mcaster1DAWCast"

# Pass 5: App bundle (with entitlements)
codesign --force --sign "$APP_SIGN_HASH" --options runtime --timestamp \
    --entitlements "$ENTITLEMENTS" "$APP_BUNDLE"

# Verify
codesign --verify --deep --strict "$APP_BUNDLE"
echo "      App bundle signed and verified."

# ── Step 5: Prepare PKG payload ──────────────────────────────────────────────
echo "[5/10] Preparing PKG payload..."
rm -rf "$PKG_ROOT" "$PKG_SCRIPTS" "$PKG_RESOURCES"
mkdir -p "$PKG_ROOT"

# Copy signed app bundle to payload root
cp -R "$APP_BUNDLE" "$PKG_ROOT/"

echo "      Payload ready."

# ── Step 6: Create post-install script ────────────────────────────────────────
echo "[6/10] Creating install scripts..."
mkdir -p "$PKG_SCRIPTS"

# Use the postinstall script from the installer directory
cp "$SCRIPT_DIR/postinstall" "$PKG_SCRIPTS/postinstall"
chmod +x "$PKG_SCRIPTS/postinstall"

echo "      Install scripts ready."

# ── Step 7: Build component + distribution package ──────────────────────────
echo "[7/10] Building PKG..."
mkdir -p "$PKG_RESOURCES" "$OUTPUT_DIR"

# Component package — installs Mcaster1DAWCast.app to /Applications
rm -f "$COMPONENT_PKG"
pkgbuild \
    --root "$PKG_ROOT" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --install-location "/Applications" \
    --scripts "$PKG_SCRIPTS" \
    "$COMPONENT_PKG"

# Copy PKG resource HTML pages
for html in welcome.html readme.html license.html conclusion.html; do
    [ -f "$SCRIPT_DIR/$html" ] && cp -f "$SCRIPT_DIR/$html" "$PKG_RESOURCES/"
done

# Use distribution.xml from the installer directory
DIST_XML="$SCRIPT_DIR/distribution.xml"
if [ ! -f "$DIST_XML" ]; then
    echo "ERROR: distribution.xml not found at: $DIST_XML"
    exit 1
fi

# Substitute version placeholders in distribution.xml
sed -e "s/INSTALLER_VERSION_FULL/$VERSION_FULL/g" \
    -e "s/INSTALLER_VERSION/$VERSION/g" \
    "$DIST_XML" > "$BUILD_DIR/distribution.xml"

# Build signed product archive (using installer cert SHA1 hash)
rm -f "$PKG_OUTPUT"
productbuild \
    --distribution "$BUILD_DIR/distribution.xml" \
    --package-path "$BUILD_DIR" \
    --resources "$PKG_RESOURCES" \
    --sign "$PKG_SIGN_HASH" \
    --timestamp \
    "$PKG_OUTPUT"

echo "      PKG created and signed: $PKG_OUTPUT"

# ── Step 8: Notarize PKG ─────────────────────────────────────────────────────
echo "[8/10] Notarizing PKG..."
xcrun notarytool submit "$PKG_OUTPUT" \
    --keychain-profile "Mcaster1" \
    --wait 2>&1
echo "      Notarization complete."

# ── Step 9: Staple notarization ticket ────────────────────────────────────────
echo "[9/10] Stapling notarization ticket..."
xcrun stapler staple "$PKG_OUTPUT" 2>&1
echo "      Stapled."

# ── Step 10: Attestation + Cleanup + Summary ─────────────────────────────────
echo "[10/10] Generating attestation and cleaning up..."

cat > "$ATTESTATION_OUTPUT" << ATTEST_EOF
Mcaster1DAWCast macOS PKG Distribution Attestation
===================================================

Package:        $(basename "$PKG_OUTPUT")
Type:           macOS Installer Package (.pkg)
Version:        $VERSION_FULL
Architecture:   arm64 (Apple Silicon)
Build Date:     $(date -u +"%Y-%m-%d %H:%M:%S UTC")
Build Host:     $(hostname)
macOS Version:  $(sw_vers -productVersion)
Qt Version:     $(qmake -query QT_VERSION 2>/dev/null || echo "6.10.2")

Install Location:
  Primary:    /Applications/Mcaster1DAWCast.app
  Portable:   ~/Mcaster1/Mcaster1DAWCast/Mcaster1DAWCast.app

Signing Identities:
  Application:  $APP_SIGN_ID
  Installer:    $PKG_SIGN_ID
  Team ID:      $TEAM_ID

Application Code Signature:
$(codesign -dv --verbose=4 "$APP_BUNDLE" 2>&1 | grep -E "Identifier|TeamIdentifier|Signature|CDHash|Runtime|Timestamp|Format")

Package Signature:
$(pkgutil --check-signature "$PKG_OUTPUT" 2>&1 | head -10)

SHA-256 Checksums:
  PKG:         $(shasum -a 256 "$PKG_OUTPUT" | awk '{print $1}')
  App Binary:  $(shasum -a 256 "$APP_BUNDLE/Contents/MacOS/Mcaster1DAWCast" | awk '{print $1}')

Entitlements:
$(codesign -d --entitlements :- "$APP_BUNDLE" 2>/dev/null | head -30)

Bundled Frameworks:
$(ls "$APP_BUNDLE/Contents/Frameworks/" 2>/dev/null | head -30)

Notarization:
$(xcrun stapler validate "$PKG_OUTPUT" 2>&1)
ATTEST_EOF

# Cleanup intermediate files
rm -rf "$PKG_ROOT" "$PKG_SCRIPTS" "$PKG_RESOURCES" "$COMPONENT_PKG"
rm -f "$BUILD_DIR/distribution.xml"

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  PKG Build Complete!                                       ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "  Output:      $PKG_OUTPUT"
echo "  Size:        $(du -h "$PKG_OUTPUT" | cut -f1)"
echo "  SHA-256:     $(shasum -a 256 "$PKG_OUTPUT" | awk '{print $1}' | cut -c1-16)..."
echo "  Attestation: $ATTESTATION_OUTPUT"
echo "╚══════════════════════════════════════════════════════════════╝"
