#!/usr/bin/env bash
# install-deps.sh — Install all build + runtime dependencies for Mcaster1DAWCast
set -euo pipefail

echo "── Mcaster1DAWCast — Dependency Installer ──"
echo ""

OS="$(uname -s)"

case "$OS" in
    Darwin)
        echo "Platform: macOS"
        echo ""

        if ! command -v brew &>/dev/null; then
            echo "ERROR: Homebrew not found. Install from https://brew.sh"
            exit 1
        fi

        echo "Installing build tools..."
        brew install autoconf automake autoconf-archive pkg-config

        echo ""
        echo "Installing required libraries..."
        brew install qt@6 portaudio ffmpeg taglib sqlite libyaml

        echo ""
        echo "Installing optional audio codecs..."
        brew install lame mpg123 libvorbis flac opus opusenc fdk-aac \
            libsndfile libsoxr speex wavpack twolame || true

        echo ""
        echo "Installing optional video codecs..."
        brew install x264 libvpx theora aom svt-av1 dav1d webp || true

        echo ""
        echo "Installing optional subtitle/text..."
        brew install libass freetype fontconfig fribidi || true

        echo ""
        echo "── Done. Now run:"
        echo "   ./autogen.sh"
        echo "   ./configure --enable-macos-gui"
        echo "   make -j\$(sysctl -n hw.ncpu)"
        ;;

    Linux)
        echo "Platform: Linux"
        echo ""

        if command -v apt &>/dev/null; then
            echo "Debian/Ubuntu detected"
            sudo apt update
            sudo apt install -y \
                build-essential autoconf automake autoconf-archive pkg-config \
                qt6-base-dev qt6-multimedia-dev libqt6svg6-dev \
                libportaudio2 portaudio19-dev \
                libavformat-dev libavcodec-dev libswresample-dev libswscale-dev libavutil-dev \
                libtag1-dev libsqlite3-dev libyaml-dev \
                libmp3lame-dev libmpg123-dev libvorbis-dev libflac-dev \
                libopus-dev libfdk-aac-dev libsndfile1-dev \
                libx264-dev libvpx-dev libtheora-dev \
                libass-dev libfreetype-dev libfontconfig-dev libfribidi-dev
        elif command -v dnf &>/dev/null; then
            echo "Fedora/RHEL detected"
            sudo dnf install -y \
                gcc-c++ autoconf automake autoconf-archive pkgconfig \
                qt6-qtbase-devel qt6-qtmultimedia-devel qt6-qtsvg-devel \
                portaudio-devel ffmpeg-devel \
                taglib-devel sqlite-devel libyaml-devel \
                lame-devel mpg123-devel libvorbis-devel flac-devel \
                opus-devel fdk-aac-free-devel libsndfile-devel \
                x264-devel libvpx-devel libtheora-devel \
                libass-devel freetype-devel fontconfig-devel fribidi-devel
        else
            echo "Unsupported package manager. Install dependencies manually."
            exit 1
        fi

        echo ""
        echo "── Done. Now run:"
        echo "   ./autogen.sh"
        echo "   ./configure"
        echo "   make -j\$(nproc)"
        ;;

    *)
        echo "Unsupported OS: $OS"
        echo "For Windows, use MSYS2 with pacman to install dependencies."
        exit 1
        ;;
esac
