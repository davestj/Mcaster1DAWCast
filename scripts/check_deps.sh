#!/usr/bin/env bash
# check_deps.sh — Verify all build dependencies are installed
set -euo pipefail

echo "── Mcaster1DAWCast — Dependency Check ──"
echo ""

MISSING=0

check_pkg() {
    local name="$1"
    local required="${2:-optional}"
    if pkg-config --exists "$name" 2>/dev/null; then
        local ver=$(pkg-config --modversion "$name" 2>/dev/null)
        printf "  %-28s %-12s %s\n" "$name" "$ver" "OK"
    else
        if [ "$required" = "required" ]; then
            printf "  %-28s %-12s %s\n" "$name" "---" "MISSING (required)"
            MISSING=$((MISSING + 1))
        else
            printf "  %-28s %-12s %s\n" "$name" "---" "not found (optional)"
        fi
    fi
}

check_bin() {
    local name="$1"
    if command -v "$name" &>/dev/null; then
        local path=$(command -v "$name")
        printf "  %-28s %s\n" "$name" "$path"
    else
        printf "  %-28s %s\n" "$name" "not found"
    fi
}

echo "Required Libraries:"
check_pkg "Qt6Core"        required
check_pkg "Qt6Gui"         required
check_pkg "Qt6Widgets"     required
check_pkg "Qt6Network"     required
check_pkg "Qt6Concurrent"  required
check_pkg "Qt6Multimedia"  required
check_pkg "portaudio-2.0"  required
check_pkg "libavformat"    required
check_pkg "libavcodec"     required
check_pkg "libswresample"  required
check_pkg "libswscale"     required
check_pkg "libavutil"      required
check_pkg "taglib"         required
check_pkg "sqlite3"        required
check_pkg "yaml-0.1"       required

echo ""
echo "Optional Audio Codecs:"
check_pkg "lame"
check_pkg "libmpg123"
check_pkg "vorbis"
check_pkg "flac"
check_pkg "opus"
check_pkg "fdk-aac"
check_pkg "sndfile"
check_pkg "soxr"
check_pkg "speex"
check_pkg "wavpack"
check_pkg "twolame"

echo ""
echo "Optional Video Codecs:"
check_pkg "x264"
check_pkg "libvpx"
check_pkg "theora"
check_pkg "aom"
check_pkg "SvtAv1Enc"
check_pkg "dav1d"
check_pkg "libwebp"

echo ""
echo "Optional Subtitle/Text:"
check_pkg "libass"
check_pkg "freetype2"
check_pkg "fontconfig"
check_pkg "fribidi"

echo ""
echo "Optional Qt Modules:"
check_pkg "Qt6Svg"
check_pkg "Qt6Charts"
check_pkg "Qt6MultimediaWidgets"

echo ""
echo "Optional Security:"
check_pkg "openssl"

echo ""
echo "External Tools:"
check_bin "ffmpeg"
check_bin "ffplay"
check_bin "yt-dlp"

echo ""
if [ $MISSING -gt 0 ]; then
    echo "ERROR: $MISSING required dependencies missing."
    echo ""
    echo "Install on macOS:  brew install qt portaudio ffmpeg taglib sqlite libyaml"
    echo "Install on Ubuntu: sudo apt install qt6-base-dev libportaudio-dev libavformat-dev libtag1-dev libsqlite3-dev libyaml-dev"
    exit 1
else
    echo "All required dependencies found."
fi
