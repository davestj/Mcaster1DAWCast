#!/bin/bash
# ──────────────────────────────────────────────────────────────────────────────
# build-dmg.sh — Mcaster1DAWCast macOS DMG Installer Builder
#
# Creates a signed, notarized DMG containing:
#   - Mcaster1DAWCast.app (drag to /Applications or use helper script)
#   - Applications symlink (drag-to-install)
#   - "Install to Home" script (installs to ~/Mcaster1/Mcaster1DAWCast/)
#
# Signing:  Developer ID Application (app bundle, hardened runtime)
# Notarize: xcrun notarytool with "Mcaster1" keychain profile
#
# Usage:
#   bash installer/macos/build-dmg.sh
#
# Prerequisites:
#   - Release build exists at build-release/bin/RELEASE/Mcaster1DAWCast.app
#   - Developer ID Application cert in keychain
#   - "Mcaster1" notarytool keychain profile stored
#   - create-dmg installed (brew install create-dmg)
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-release"
APP_NAME="Mcaster1DAWCast"
IDENTIFIER="com.mcaster1.Mcaster1DAWCast"
STAGING_DIR="$BUILD_DIR/dmg-staging"
OUTPUT_DIR="$SCRIPT_DIR"

# Read version from VERSION.txt
VERSION_FILE="$PROJECT_DIR/VERSION.txt"
if [ -f "$VERSION_FILE" ]; then
    VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"
else
    echo "WARNING: VERSION.txt not found at $VERSION_FILE — defaulting to 1.0.0-alpha"
    VERSION="1.0.0-alpha"
fi

DMG_OUTPUT="$OUTPUT_DIR/${APP_NAME}-${VERSION}-macOS-arm64.dmg"
ATTESTATION_OUTPUT="$OUTPUT_DIR/${APP_NAME}-${VERSION}-macOS-arm64.dmg.attestation.txt"

# Signing identities (by SHA1 hash to avoid ambiguity)
APP_SIGN_HASH="040C503B906961B9D848F989D778ED754D3C2770"
APP_SIGN_ID="Developer ID Application: David St John (FCA38UPLY3)"
TEAM_ID="FCA38UPLY3"

# Source paths
APP_BUNDLE="$BUILD_DIR/bin/RELEASE/${APP_NAME}.app"
ENTITLEMENTS="$SCRIPT_DIR/entitlements.plist"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  Mcaster1DAWCast macOS DMG Builder                         ║"
echo "║  Version:  $VERSION                                        ║"
echo "║  Arch:     arm64 (Apple Silicon)                           ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ── Step 1: Verify build ──────────────────────────────────────────────────────
if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: App bundle not found at: $APP_BUNDLE"
    echo "Build first:"
    echo "  cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \\"
    echo "    -DBUILD_TESTS=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt"
    echo "  cmake --build build-release"
    exit 1
fi
echo "[1/8] Build verified: $APP_BUNDLE"

# ── Step 2: Run macdeployqt ──────────────────────────────────────────────────
echo "[2/8] Running macdeployqt..."
MACDEPLOYQT=$(command -v macdeployqt || echo "/opt/homebrew/bin/macdeployqt")
"$MACDEPLOYQT" "$APP_BUNDLE" -always-overwrite 2>&1 | tail -5 || true
echo "     macdeployqt complete."

# ── Step 3: Copy resources into app bundle ───────────────────────────────────
echo "[3/8] Copying resources..."
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

echo "     Resources copied."

# ── Step 4: Code sign app bundle (inside-out, hardened runtime) ──────────────
echo "[4/8] Code signing (inside-out, hardened runtime + timestamp)..."

# Pass 1: Frameworks — individual dylibs first
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
echo "     App bundle signed and verified."

# ── Step 5: Create DMG staging ──────────────────────────────────────────────
echo "[5/8] Creating DMG..."
rm -f "$DMG_OUTPUT"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR" "$OUTPUT_DIR"

# Copy signed app to staging
cp -R "$APP_BUNDLE" "$STAGING_DIR/"

# Install-to-home helper script
cat > "$STAGING_DIR/Install to Home.command" << 'INSTALL_EOF'
#!/bin/bash
# Mcaster1DAWCast — Install to ~/Mcaster1/Mcaster1DAWCast/
DEST="$HOME/Mcaster1/Mcaster1DAWCast"
SRC="$(cd "$(dirname "$0")" && pwd)/Mcaster1DAWCast.app"

if [ ! -d "$SRC" ]; then
    echo "Error: Mcaster1DAWCast.app not found next to this script."
    echo "Make sure you're running this from the mounted DMG."
    exit 1
fi

echo ""
echo "Installing Mcaster1DAWCast to $DEST ..."
echo ""

# Remove old install if present
[ -d "$DEST/Mcaster1DAWCast.app" ] && rm -rf "$DEST/Mcaster1DAWCast.app"

mkdir -p "$DEST"
cp -R "$SRC" "$DEST/"

# Create Applications symlink (may need admin)
if [ ! -e "/Applications/Mcaster1DAWCast.app" ]; then
    ln -sf "$DEST/Mcaster1DAWCast.app" "/Applications/Mcaster1DAWCast.app" 2>/dev/null || \
    echo "  Note: /Applications symlink requires admin. Drag from ~/Mcaster1 instead."
fi

echo ""
echo "  Installed: $DEST/Mcaster1DAWCast.app"
echo "  Launch from Applications, Spotlight, or Launchpad."
echo ""
echo "  You can close this window."
INSTALL_EOF
chmod +x "$STAGING_DIR/Install to Home.command"

# App icon for DMG volume
VOLICON=""
if [ -f "$PROJECT_DIR/image_resources/Mcaster1DAWCast.icns" ]; then
    VOLICON="--volicon $PROJECT_DIR/image_resources/Mcaster1DAWCast.icns"
fi

# Build DMG with create-dmg
create-dmg \
    --volname "$APP_NAME $VERSION" \
    $VOLICON \
    --window-pos 200 120 \
    --window-size 660 400 \
    --icon-size 80 \
    --icon "$APP_NAME.app" 160 170 \
    --app-drop-link 500 170 \
    --icon "Install to Home.command" 330 330 \
    --codesign "$APP_SIGN_ID" \
    "$DMG_OUTPUT" \
    "$STAGING_DIR/" || true

if [ ! -f "$DMG_OUTPUT" ]; then
    echo "ERROR: DMG creation failed!"
    exit 1
fi
rm -rf "$STAGING_DIR"
echo "     DMG created: $DMG_OUTPUT"

# ── Step 6: Notarize DMG ────────────────────────────────────────────────────
echo "[6/8] Notarizing DMG..."
xcrun notarytool submit "$DMG_OUTPUT" \
    --keychain-profile "Mcaster1" \
    --wait 2>&1
echo "     Notarization complete."

# ── Step 7: Staple notarization ticket ────────────────────────────────────────
echo "[7/8] Stapling notarization ticket..."
xcrun stapler staple "$DMG_OUTPUT" 2>&1
echo "     Stapled."

# ── Step 8: Attestation + Summary ────────────────────────────────────────────
echo "[8/8] Generating attestation..."

cat > "$ATTESTATION_OUTPUT" << ATTEST_EOF
Mcaster1DAWCast macOS DMG Distribution Attestation
===================================================

Package:        $(basename "$DMG_OUTPUT")
Type:           macOS Disk Image (.dmg)
Version:        $VERSION
Architecture:   arm64 (Apple Silicon)
Build Date:     $(date -u +"%Y-%m-%d %H:%M:%S UTC")
Build Host:     $(hostname)
macOS Version:  $(sw_vers -productVersion)
Qt Version:     $(qmake -query QT_VERSION 2>/dev/null || echo "6.10.2")

Signing Identity:
  Application:  $APP_SIGN_ID
  Team ID:      $TEAM_ID

Application Code Signature:
$(codesign -dv --verbose=4 "$APP_BUNDLE" 2>&1 | grep -E "Identifier|TeamIdentifier|Signature|CDHash|Runtime|Timestamp|Format")

SHA-256 Checksums:
  DMG:          $(shasum -a 256 "$DMG_OUTPUT" | awk '{print $1}')
  App Binary:   $(shasum -a 256 "$APP_BUNDLE/Contents/MacOS/Mcaster1DAWCast" | awk '{print $1}')

Entitlements:
$(codesign -d --entitlements :- "$APP_BUNDLE" 2>/dev/null | head -30)

Bundled Frameworks:
$(ls "$APP_BUNDLE/Contents/Frameworks/" 2>/dev/null | head -30)

Notarization:
$(xcrun stapler validate "$DMG_OUTPUT" 2>&1)
ATTEST_EOF

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  DMG Build Complete!                                       ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "  Output:      $DMG_OUTPUT"
echo "  Size:        $(du -h "$DMG_OUTPUT" | cut -f1)"
echo "  SHA-256:     $(shasum -a 256 "$DMG_OUTPUT" | awk '{print $1}' | cut -c1-16)..."
echo "  Attestation: $ATTESTATION_OUTPUT"
echo "╚══════════════════════════════════════════════════════════════╝"
