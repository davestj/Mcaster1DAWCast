dnl configure.ac — Mcaster1DAWCast (Multi-Channel DAW for Broadcasting) v1.0.0-alpha
dnl
dnl ./autogen.sh  then  ./configure --enable-macos-gui  then  make -j$(sysctl -n hw.ncpu)

AC_PREREQ([2.69])
AC_INIT([mcaster1dawcast], [1.0.0-alpha], [davestj@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AC_CONFIG_SRCDIR([src/DAWCast/main.cpp])
AC_CONFIG_HEADERS([config.h])

dnl ── Language / C++17 ────────────────────────────────────────────────────────
AC_PROG_CXX
AC_PROG_CC
AC_PROG_OBJCXX
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])

dnl ── Basic OS / library checks ──────────────────────────────────────────────
AC_CHECK_HEADERS([unistd.h pthread.h])
AC_SEARCH_LIBS([pthread_create], [pthread], [],
    [AC_MSG_ERROR([pthreads not found])])

dnl ── Extend PKG_CONFIG_PATH for Homebrew / /usr/local ───────────────────────
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
export PKG_CONFIG_PATH

dnl ═══════════════════════════════════════════════════════════════════════════
dnl  REQUIRED LIBRARIES
dnl ═══════════════════════════════════════════════════════════════════════════

dnl ── libyaml (required — YAML config parsing) ───────────────────────────────
have_libyaml=no
PKG_CHECK_MODULES([LIBYAML], [yaml-0.1],
    [have_libyaml=yes
     AC_DEFINE([HAVE_LIBYAML], [1], [libyaml available])],
    [AC_CHECK_HEADER([yaml.h],
        [AC_CHECK_LIB([yaml], [yaml_parser_initialize],
            [have_libyaml=yes
             LIBYAML_LIBS="-lyaml"
             LIBYAML_CFLAGS=""
             AC_SUBST([LIBYAML_LIBS])
             AC_SUBST([LIBYAML_CFLAGS])],
            [AC_MSG_ERROR([libyaml not found. Install: brew install libyaml])])],
        [AC_MSG_ERROR([yaml.h not found. Install: brew install libyaml])])])

dnl ── SQLite3 (required — project and media database) ────────────────────────
PKG_CHECK_MODULES([SQLITE3], [sqlite3 >= 3.24.0],
    [AC_DEFINE([HAVE_SQLITE3], [1], [SQLite3 available])],
    [AC_MSG_ERROR([SQLite3 >= 3.24.0 not found. Install: brew install sqlite3])])

dnl ── PortAudio (required — audio output) ────────────────────────────────────
have_portaudio=no
PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0],
    [have_portaudio=yes
     AC_DEFINE([HAVE_PORTAUDIO], [1], [PortAudio available])],
    [AC_MSG_ERROR([PortAudio not found. Install: brew install portaudio])])

dnl ── TagLib (required — metadata reading) ────────────────────────────────────
have_taglib=no
PKG_CHECK_MODULES([TAGLIB], [taglib >= 1.11],
    [have_taglib=yes
     AC_DEFINE([HAVE_TAGLIB], [1], [TagLib available])],
    [AC_MSG_ERROR([TagLib >= 1.11 not found. Install: brew install taglib])])

dnl ── FFmpeg (required — multi-format decode/encode + video pipeline) ────────
have_avformat=no
PKG_CHECK_MODULES([AVFORMAT], [libavformat libavcodec libswresample libswscale libavutil],
    [have_avformat=yes
     AC_DEFINE([HAVE_AVFORMAT], [1], [FFmpeg avformat/avcodec/swresample/swscale available])],
    [AC_MSG_ERROR([FFmpeg libs not found. Install: brew install ffmpeg])])
AM_CONDITIONAL([HAVE_AVFORMAT], [test "x$have_avformat" = "xyes"])

dnl ═══════════════════════════════════════════════════════════════════════════
dnl  OPTIONAL AUDIO CODEC LIBRARIES
dnl ═══════════════════════════════════════════════════════════════════════════

dnl ── libmpg123 (optional — MP3 decoding) ────────────────────────────────────
have_mpg123=no
PKG_CHECK_MODULES([MPG123], [libmpg123],
    [have_mpg123=yes
     AC_DEFINE([HAVE_MPG123], [1], [libmpg123 available for MP3 decoding])],
    [AC_MSG_NOTICE([libmpg123 not found — MP3 decoding will use FFmpeg only])])
AM_CONDITIONAL([HAVE_MPG123], [test "x$have_mpg123" = "xyes"])

dnl ── LAME (optional — MP3 encoding) ────────────────────────────────────────
have_lame=no
PKG_CHECK_MODULES([LAME], [lame],
    [have_lame=yes
     AC_DEFINE([HAVE_LAME], [1], [LAME MP3 encoder available])],
    [AC_CHECK_HEADER([lame/lame.h],
        [AC_CHECK_LIB([mp3lame], [lame_init],
            [have_lame=yes
             LAME_LIBS="-lmp3lame"
             LAME_CFLAGS=""
             AC_SUBST([LAME_LIBS])
             AC_SUBST([LAME_CFLAGS])
             AC_DEFINE([HAVE_LAME], [1], [LAME MP3 encoder available])],
            [AC_MSG_NOTICE([LAME not found — MP3 encoding disabled])])],
        [AC_MSG_NOTICE([lame/lame.h not found — MP3 encoding disabled])])])
AM_CONDITIONAL([HAVE_LAME], [test "x$have_lame" = "xyes"])

dnl ── Vorbis (optional — Ogg Vorbis encoding) ──────────────────────────────
have_vorbis=no
PKG_CHECK_MODULES([VORBIS], [vorbis vorbisenc vorbisfile ogg],
    [have_vorbis=yes
     AC_DEFINE([HAVE_VORBIS], [1], [Vorbis encoder available])],
    [AC_MSG_NOTICE([libvorbis not found — Vorbis support disabled])])
AM_CONDITIONAL([HAVE_VORBIS], [test "x$have_vorbis" = "xyes"])

dnl ── FLAC (optional — FLAC encoding) ──────────────────────────────────────
have_flac=no
PKG_CHECK_MODULES([FLAC], [flac],
    [have_flac=yes
     AC_DEFINE([HAVE_FLAC], [1], [FLAC encoder available])],
    [AC_MSG_NOTICE([libFLAC not found — FLAC support disabled])])
AM_CONDITIONAL([HAVE_FLAC], [test "x$have_flac" = "xyes"])

dnl ── Opus (optional — Opus encoding) ──────────────────────────────────────
have_opus=no
PKG_CHECK_MODULES([OPUS], [opus libopusenc],
    [have_opus=yes
     AC_DEFINE([HAVE_OPUS], [1], [Opus encoder available])],
    [AC_MSG_NOTICE([libopus not found — Opus support disabled])])
AM_CONDITIONAL([HAVE_OPUS], [test "x$have_opus" = "xyes"])

dnl ── fdk-aac (optional — AAC encoding) ─────────────────────────────────────
have_fdkaac=no
PKG_CHECK_MODULES([FDKAAC], [fdk-aac],
    [have_fdkaac=yes
     AC_DEFINE([HAVE_FDKAAC], [1], [FDK-AAC encoder available])],
    [AC_MSG_NOTICE([fdk-aac not found — AAC support disabled])])
AM_CONDITIONAL([HAVE_FDKAAC], [test "x$have_fdkaac" = "xyes"])

dnl ── libsndfile (optional — audio file I/O) ────────────────────────────────
have_sndfile=no
PKG_CHECK_MODULES([SNDFILE], [sndfile],
    [have_sndfile=yes
     AC_DEFINE([HAVE_SNDFILE], [1], [libsndfile available for audio file I/O])],
    [AC_MSG_NOTICE([libsndfile not found — audio file I/O will use FFmpeg only])])
AM_CONDITIONAL([HAVE_SNDFILE], [test "x$have_sndfile" = "xyes"])

dnl ── libsoxr (optional — high-quality resampling) ──────────────────────────
have_soxr=no
PKG_CHECK_MODULES([SOXR], [soxr],
    [have_soxr=yes
     AC_DEFINE([HAVE_SOXR], [1], [libsoxr available for high-quality resampling])],
    [AC_MSG_NOTICE([libsoxr not found — resampling will use FFmpeg swresample only])])
AM_CONDITIONAL([HAVE_SOXR], [test "x$have_soxr" = "xyes"])

dnl ── Speex (optional — Speex codec) ────────────────────────────────────────
have_speex=no
PKG_CHECK_MODULES([SPEEX], [speex speexdsp],
    [have_speex=yes
     AC_DEFINE([HAVE_SPEEX], [1], [Speex codec available])],
    [AC_MSG_NOTICE([Speex not found — Speex codec disabled])])
AM_CONDITIONAL([HAVE_SPEEX], [test "x$have_speex" = "xyes"])

dnl ── WavPack (optional — WavPack lossless/lossy codec) ─────────────────────
have_wavpack=no
PKG_CHECK_MODULES([WAVPACK], [wavpack],
    [have_wavpack=yes
     AC_DEFINE([HAVE_WAVPACK], [1], [WavPack codec available])],
    [AC_MSG_NOTICE([WavPack not found — WavPack support disabled])])
AM_CONDITIONAL([HAVE_WAVPACK], [test "x$have_wavpack" = "xyes"])

dnl ── TwoLAME (optional — MP2 encoding) ─────────────────────────────────────
have_twolame=no
PKG_CHECK_MODULES([TWOLAME], [twolame],
    [have_twolame=yes
     AC_DEFINE([HAVE_TWOLAME], [1], [TwoLAME MP2 encoder available])],
    [AC_MSG_NOTICE([TwoLAME not found — MP2 encoding disabled])])
AM_CONDITIONAL([HAVE_TWOLAME], [test "x$have_twolame" = "xyes"])

dnl ── libshine (optional — fixed-point MP3 encoding) ────────────────────────
have_shine=no
AC_CHECK_HEADER([shine/layer3.h],
    [AC_CHECK_LIB([shine], [shine_encode_buffer_interleaved],
        [have_shine=yes
         SHINE_LIBS="-lshine"
         SHINE_CFLAGS=""
         AC_SUBST([SHINE_LIBS])
         AC_SUBST([SHINE_CFLAGS])
         AC_DEFINE([HAVE_SHINE], [1], [libshine fixed-point MP3 encoder available])],
        [AC_MSG_NOTICE([libshine not found — fixed-point MP3 encoding disabled])])],
    [AC_MSG_NOTICE([shine/layer3.h not found — fixed-point MP3 encoding disabled])])
AM_CONDITIONAL([HAVE_SHINE], [test "x$have_shine" = "xyes"])

dnl ── libgsm (optional — GSM 06.10 codec) ───────────────────────────────────
have_gsm=no
AC_CHECK_HEADER([gsm/gsm.h],
    [AC_CHECK_LIB([gsm], [gsm_create],
        [have_gsm=yes
         GSM_LIBS="-lgsm"
         GSM_CFLAGS=""
         AC_SUBST([GSM_LIBS])
         AC_SUBST([GSM_CFLAGS])
         AC_DEFINE([HAVE_GSM], [1], [libgsm GSM 06.10 codec available])],
        [AC_MSG_NOTICE([libgsm not found — GSM codec disabled])])],
    [AC_CHECK_HEADER([gsm.h],
        [AC_CHECK_LIB([gsm], [gsm_create],
            [have_gsm=yes
             GSM_LIBS="-lgsm"
             GSM_CFLAGS=""
             AC_SUBST([GSM_LIBS])
             AC_SUBST([GSM_CFLAGS])
             AC_DEFINE([HAVE_GSM], [1], [libgsm GSM 06.10 codec available])],
            [AC_MSG_NOTICE([libgsm not found — GSM codec disabled])])],
        [AC_MSG_NOTICE([gsm.h not found — GSM codec disabled])])])
AM_CONDITIONAL([HAVE_GSM], [test "x$have_gsm" = "xyes"])

dnl ── OpenCore AMR (optional — AMR-NB + AMR-WB codecs) ──────────────────────
have_opencore_amr=no
PKG_CHECK_MODULES([OPENCORE_AMR], [opencore-amrnb opencore-amrwb],
    [have_opencore_amr=yes
     AC_DEFINE([HAVE_OPENCORE_AMR], [1], [OpenCore AMR-NB/WB codecs available])],
    [AC_MSG_NOTICE([OpenCore AMR not found — AMR codec support disabled])])
AM_CONDITIONAL([HAVE_OPENCORE_AMR], [test "x$have_opencore_amr" = "xyes"])

dnl ═══════════════════════════════════════════════════════════════════════════
dnl  OPTIONAL VIDEO CODEC LIBRARIES
dnl ═══════════════════════════════════════════════════════════════════════════

dnl ── x264 (optional — H.264 encoding) ──────────────────────────────────────
have_x264=no
PKG_CHECK_MODULES([X264], [x264],
    [have_x264=yes
     AC_DEFINE([HAVE_X264], [1], [x264 H.264 encoder available])],
    [AC_MSG_NOTICE([x264 not found — H.264 encoding disabled])])
AM_CONDITIONAL([HAVE_X264], [test "x$have_x264" = "xyes"])

dnl ── libvpx (optional — VP8/VP9 encoding) ──────────────────────────────────
have_vpx=no
PKG_CHECK_MODULES([VPX], [libvpx],
    [have_vpx=yes
     AC_DEFINE([HAVE_VPX], [1], [libvpx VP8/VP9 encoder available])],
    [AC_MSG_NOTICE([libvpx not found — VP8/VP9 encoding disabled])])
AM_CONDITIONAL([HAVE_VPX], [test "x$have_vpx" = "xyes"])

dnl ── libtheora (optional — Theora encoding) ────────────────────────────────
have_theora=no
PKG_CHECK_MODULES([THEORA], [theora theoraenc theoradec],
    [have_theora=yes
     AC_DEFINE([HAVE_THEORA], [1], [libtheora Theora encoder/decoder available])],
    [AC_MSG_NOTICE([libtheora not found — Theora encoding disabled])])
AM_CONDITIONAL([HAVE_THEORA], [test "x$have_theora" = "xyes"])

dnl ── libaom (optional — AV1 codec) ─────────────────────────────────────────
have_aom=no
PKG_CHECK_MODULES([AOM], [aom],
    [have_aom=yes
     AC_DEFINE([HAVE_AOM], [1], [libaom AV1 codec available])],
    [AC_MSG_NOTICE([libaom not found — AV1 encoding/decoding via libaom disabled])])
AM_CONDITIONAL([HAVE_AOM], [test "x$have_aom" = "xyes"])

dnl ── SVT-AV1 (optional — AV1 encoder) ──────────────────────────────────────
have_svtav1=no
PKG_CHECK_MODULES([SVTAV1], [SvtAv1Enc],
    [have_svtav1=yes
     AC_DEFINE([HAVE_SVTAV1], [1], [SVT-AV1 encoder available])],
    [AC_MSG_NOTICE([SVT-AV1 not found — SVT-AV1 encoding disabled])])
AM_CONDITIONAL([HAVE_SVTAV1], [test "x$have_svtav1" = "xyes"])

dnl ── dav1d (optional — AV1 decoder) ────────────────────────────────────────
have_dav1d=no
PKG_CHECK_MODULES([DAV1D], [dav1d],
    [have_dav1d=yes
     AC_DEFINE([HAVE_DAV1D], [1], [dav1d AV1 decoder available])],
    [AC_MSG_NOTICE([dav1d not found — AV1 decoding via dav1d disabled])])
AM_CONDITIONAL([HAVE_DAV1D], [test "x$have_dav1d" = "xyes"])

dnl ── libwebp (optional — WebP image codec) ─────────────────────────────────
have_webp=no
PKG_CHECK_MODULES([WEBP], [libwebp],
    [have_webp=yes
     AC_DEFINE([HAVE_WEBP], [1], [libwebp WebP image codec available])],
    [AC_MSG_NOTICE([libwebp not found — WebP image support disabled])])
AM_CONDITIONAL([HAVE_WEBP], [test "x$have_webp" = "xyes"])

dnl ═══════════════════════════════════════════════════════════════════════════
dnl  OPTIONAL SUBTITLE / TEXT RENDERING LIBRARIES
dnl ═══════════════════════════════════════════════════════════════════════════

dnl ── libass (optional — ASS/SSA subtitle rendering) ────────────────────────
have_libass=no
PKG_CHECK_MODULES([LIBASS], [libass],
    [have_libass=yes
     AC_DEFINE([HAVE_LIBASS], [1], [libass ASS subtitle renderer available])],
    [AC_MSG_NOTICE([libass not found — ASS subtitle rendering disabled])])
AM_CONDITIONAL([HAVE_LIBASS], [test "x$have_libass" = "xyes"])

dnl ── FreeType (optional — text rendering for subtitle overlays) ─────────────
have_freetype=no
PKG_CHECK_MODULES([FREETYPE], [freetype2],
    [have_freetype=yes
     AC_DEFINE([HAVE_FREETYPE], [1], [FreeType text rendering available])],
    [AC_MSG_NOTICE([FreeType not found — text overlay rendering disabled])])
AM_CONDITIONAL([HAVE_FREETYPE], [test "x$have_freetype" = "xyes"])

dnl ── Fontconfig (optional — font discovery) ─────────────────────────────────
have_fontconfig=no
PKG_CHECK_MODULES([FONTCONFIG], [fontconfig],
    [have_fontconfig=yes
     AC_DEFINE([HAVE_FONTCONFIG], [1], [Fontconfig font discovery available])],
    [AC_MSG_NOTICE([Fontconfig not found — system font discovery disabled])])
AM_CONDITIONAL([HAVE_FONTCONFIG], [test "x$have_fontconfig" = "xyes"])

dnl ── FriBidi (optional — bidirectional text layout) ─────────────────────────
have_fribidi=no
PKG_CHECK_MODULES([FRIBIDI], [fribidi],
    [have_fribidi=yes
     AC_DEFINE([HAVE_FRIBIDI], [1], [FriBidi bidirectional text available])],
    [AC_MSG_NOTICE([FriBidi not found — bidirectional text layout disabled])])
AM_CONDITIONAL([HAVE_FRIBIDI], [test "x$have_fribidi" = "xyes"])

dnl ── libzvbi (optional — teletext decoder) ──────────────────────────────────
have_zvbi=no
PKG_CHECK_MODULES([ZVBI], [zvbi-0.2],
    [have_zvbi=yes
     AC_DEFINE([HAVE_ZVBI], [1], [libzvbi teletext decoder available])],
    [AC_MSG_NOTICE([libzvbi not found — teletext support disabled])])
AM_CONDITIONAL([HAVE_ZVBI], [test "x$have_zvbi" = "xyes"])

dnl ═══════════════════════════════════════════════════════════════════════════
dnl  OPTIONAL SECURITY / NETWORKING
dnl ═══════════════════════════════════════════════════════════════════════════

dnl ── OpenSSL (optional — HTTPS stream connections) ──────────────────────────
have_openssl=no
PKG_CHECK_MODULES([OPENSSL], [openssl >= 1.1],
    [have_openssl=yes
     AC_DEFINE([HAVE_OPENSSL], [1], [OpenSSL available for TLS])],
    [AC_MSG_NOTICE([OpenSSL not found — will fall back to SecureTransport])])
AM_CONDITIONAL([HAVE_OPENSSL], [test "x$have_openssl" = "xyes"])

dnl ═══════════════════════════════════════════════════════════════════════════
dnl  macOS Qt 6 GUI (required for now — macOS-first build)
dnl ═══════════════════════════════════════════════════════════════════════════
AC_ARG_ENABLE([macos-gui],
    [AS_HELP_STRING([--enable-macos-gui], [Build macOS Qt 6 GUI application])],
    [enable_macos_gui=$enableval], [enable_macos_gui=no])

have_qt6=no
AS_IF([test "x$enable_macos_gui" = "xyes"], [
    PKG_CHECK_MODULES([QT6WIDGETS], [Qt6Widgets >= 6.5],
        [have_qt6=yes],
        [AC_MSG_ERROR([Qt 6 Widgets >= 6.5 not found. Install: brew install qt])])
    PKG_CHECK_MODULES([QT6CORE],       [Qt6Core       >= 6.5])
    PKG_CHECK_MODULES([QT6GUI],        [Qt6Gui        >= 6.5])
    PKG_CHECK_MODULES([QT6NETWORK],    [Qt6Network    >= 6.5])
    PKG_CHECK_MODULES([QT6CONCURRENT], [Qt6Concurrent >= 6.5])

    dnl Qt6Multimedia + Qt6MultimediaWidgets (required — DAW media pipeline)
    dnl Provides QMediaPlayer, QVideoWidget, QAudioOutput, QMediaRecorder.
    have_qt6multimedia=no
    PKG_CHECK_MODULES([QT6MULTIMEDIA],
        [Qt6Multimedia >= 6.5 Qt6MultimediaWidgets >= 6.5],
        [have_qt6multimedia=yes
         AC_DEFINE([HAVE_QT6MULTIMEDIA], [1],
                   [Qt6 Multimedia and MultimediaWidgets available])
         AC_SUBST([QT6MULTIMEDIA_CFLAGS])
         AC_SUBST([QT6MULTIMEDIA_LIBS])],
        [AC_MSG_ERROR([Qt6Multimedia >= 6.5 not found. Install: brew install qt])])

    dnl Qt6WebEngineWidgets (optional — rich HTML5 media browser)
    dnl Falls back to plain QListWidget when unavailable.
    have_qt6webengine=no
    PKG_CHECK_MODULES([QT6WEBENGINE], [Qt6WebEngineWidgets >= 6.5],
        [have_qt6webengine=yes
         AC_DEFINE([HAVE_QT6WEBENGINE], [1],
                   [Qt6 WebEngineWidgets available for HTML5 media browser])
         AC_SUBST([QT6WEBENGINE_CFLAGS])
         AC_SUBST([QT6WEBENGINE_LIBS])],
        [AC_MSG_NOTICE([Qt6WebEngineWidgets not found — media browser uses QListWidget fallback])])
    AM_CONDITIONAL([HAVE_QT6WEBENGINE], [test "x$have_qt6webengine" = "xyes"])

    dnl Qt6Svg (optional — SVG icon/image rendering)
    have_qt6svg=no
    PKG_CHECK_MODULES([QT6SVG], [Qt6Svg >= 6.5],
        [have_qt6svg=yes
         AC_DEFINE([HAVE_QT6SVG], [1],
                   [Qt6 Svg available for SVG rendering])
         AC_SUBST([QT6SVG_CFLAGS])
         AC_SUBST([QT6SVG_LIBS])],
        [AC_MSG_NOTICE([Qt6Svg not found — SVG rendering disabled])])
    AM_CONDITIONAL([HAVE_QT6SVG], [test "x$have_qt6svg" = "xyes"])

    dnl Qt6Charts (optional — metering and analysis visualizations)
    have_qt6charts=no
    PKG_CHECK_MODULES([QT6CHARTS], [Qt6Charts >= 6.5],
        [have_qt6charts=yes
         AC_DEFINE([HAVE_QT6CHARTS], [1],
                   [Qt6 Charts available for metering visualizations])
         AC_SUBST([QT6CHARTS_CFLAGS])
         AC_SUBST([QT6CHARTS_LIBS])],
        [AC_MSG_NOTICE([Qt6Charts not found — chart-based metering disabled])])
    AM_CONDITIONAL([HAVE_QT6CHARTS], [test "x$have_qt6charts" = "xyes"])

    dnl yt-dlp binary path (optional — YouTube/Vimeo URL resolution)
    YTDLP_BIN=""
    AC_PATH_PROG([YTDLP_BIN], [yt-dlp], [],
        [/opt/homebrew/bin:/usr/local/bin:$PATH])
    AS_IF([test -n "$YTDLP_BIN"],
        [AC_DEFINE_UNQUOTED([YTDLP_DEFAULT_PATH], ["$YTDLP_BIN"],
                            [Default path to yt-dlp binary])],
        [AC_DEFINE([YTDLP_DEFAULT_PATH], ["/opt/homebrew/bin/yt-dlp"],
                   [Default path to yt-dlp binary])])

    dnl ffmpeg binary path (optional — transcoding pipe)
    FFMPEG_BIN=""
    AC_PATH_PROG([FFMPEG_BIN], [ffmpeg], [],
        [/opt/homebrew/bin:/usr/local/bin:$PATH])
    AS_IF([test -n "$FFMPEG_BIN"],
        [AC_DEFINE_UNQUOTED([FFMPEG_DEFAULT_PATH], ["$FFMPEG_BIN"],
                            [Default path to ffmpeg binary])],
        [AC_DEFINE([FFMPEG_DEFAULT_PATH], ["/opt/homebrew/bin/ffmpeg"],
                   [Default path to ffmpeg binary])])

    dnl ffplay binary path (optional — media preview playback)
    FFPLAY_BIN=""
    AC_PATH_PROG([FFPLAY_BIN], [ffplay], [],
        [/opt/homebrew/bin:/usr/local/bin:$PATH])
    AS_IF([test -n "$FFPLAY_BIN"],
        [AC_DEFINE_UNQUOTED([FFPLAY_DEFAULT_PATH], ["$FFPLAY_BIN"],
                            [Default path to ffplay binary])],
        [AC_DEFINE([FFPLAY_DEFAULT_PATH], ["/opt/homebrew/bin/ffplay"],
                   [Default path to ffplay binary])])

    dnl Locate moc and rcc from Qt installation
    QT6_HOST_BINS=`$PKG_CONFIG --variable=host_bins Qt6Core 2>/dev/null`
    if test -z "$QT6_HOST_BINS" || test ! -x "$QT6_HOST_BINS/moc"; then
        QT6_HOST_BINS=`$PKG_CONFIG --variable=libexecdir Qt6Core 2>/dev/null`
    fi
    if test -z "$QT6_HOST_BINS" || test ! -x "$QT6_HOST_BINS/moc"; then
        QT6_HOST_BINS=`$PKG_CONFIG --variable=prefix Qt6Core`/share/qt/libexec
    fi
    QT6_MOC="${QT6_HOST_BINS}/moc"
    QT6_RCC="${QT6_HOST_BINS}/rcc"
    if test ! -x "$QT6_MOC"; then
        AC_MSG_ERROR([Qt6 moc not found at $QT6_MOC. Check your Qt installation.])
    fi
    AC_SUBST([QT6_MOC])
    AC_SUBST([QT6_RCC])
    AC_SUBST([QT6WIDGETS_CFLAGS])
    AC_SUBST([QT6WIDGETS_LIBS])
    AC_SUBST([QT6CORE_CFLAGS])
    AC_SUBST([QT6CORE_LIBS])
    AC_SUBST([QT6GUI_CFLAGS])
    AC_SUBST([QT6GUI_LIBS])
    AC_SUBST([QT6NETWORK_CFLAGS])
    AC_SUBST([QT6NETWORK_LIBS])
    AC_SUBST([QT6CONCURRENT_CFLAGS])
    AC_SUBST([QT6CONCURRENT_LIBS])

    dnl macOS system frameworks for audio/video output + media integration
    MACOS_FRAMEWORKS="-framework CoreAudio -framework AudioToolbox -framework AVFoundation -framework CoreMedia -framework Cocoa -framework MediaPlayer -framework UserNotifications -framework Accelerate -framework Security -framework QuartzCore -framework CoreImage -framework CoreBluetooth -framework IOBluetooth -framework Network -framework AVKit -framework VideoToolbox -framework OpenGL -framework Metal -framework CoreGraphics -liconv"
    AC_SUBST([MACOS_FRAMEWORKS])
    AC_DEFINE([DAWCAST_MACOS_GUI], [1], [Building macOS Qt 6 GUI])
])
AM_CONDITIONAL([ENABLE_MACOS_GUI], [test "x$enable_macos_gui" = "xyes"])
AM_CONDITIONAL([HAVE_QT6WEBENGINE], [test "x${have_qt6webengine:-no}" = "xyes"])
AM_CONDITIONAL([HAVE_QT6SVG], [test "x${have_qt6svg:-no}" = "xyes"])
AM_CONDITIONAL([HAVE_QT6CHARTS], [test "x${have_qt6charts:-no}" = "xyes"])
AM_CONDITIONAL([HAVE_QT6MULTIMEDIA], [test "x${have_qt6multimedia:-no}" = "xyes"])

dnl ── Output ─────────────────────────────────────────────────────────────────
AC_CONFIG_FILES([
    Makefile
    src/Makefile
    src/DAWCast/Makefile
    src/EditorStudio/Makefile
])
AC_OUTPUT

dnl ── Summary ────────────────────────────────────────────────────────────────
echo ""
echo "── Mcaster1DAWCast v1.0.0-alpha — Configuration Summary ──"
echo ""
echo "  Platform:"
echo "     macOS GUI : ${enable_macos_gui}"
echo "     Qt 6      : ${have_qt6}"
echo ""
echo "  Required:"
echo "     PortAudio : ${have_portaudio}"
echo "     TagLib    : ${have_taglib}"
echo "     SQLite3   : yes"
echo "     libyaml   : ${have_libyaml}"
echo "     FFmpeg    : ${have_avformat}"
echo ""
echo "  Qt Modules:"
echo "     Qt Multimedia : ${have_qt6multimedia:-no}"
echo "     Qt WebEngine  : ${have_qt6webengine:-no}"
echo "     Qt Svg        : ${have_qt6svg:-no}"
echo "     Qt Charts     : ${have_qt6charts:-no}"
echo ""
echo "  Audio Codecs (optional):"
echo "     libmpg123     : ${have_mpg123}"
echo "     LAME          : ${have_lame}"
echo "     Vorbis        : ${have_vorbis}"
echo "     FLAC          : ${have_flac}"
echo "     Opus          : ${have_opus}"
echo "     FDK-AAC       : ${have_fdkaac}"
echo "     libsndfile    : ${have_sndfile}"
echo "     libsoxr       : ${have_soxr}"
echo "     Speex         : ${have_speex}"
echo "     WavPack       : ${have_wavpack}"
echo "     TwoLAME       : ${have_twolame}"
echo "     libshine      : ${have_shine}"
echo "     libgsm        : ${have_gsm}"
echo "     OpenCore AMR  : ${have_opencore_amr}"
echo ""
echo "  Video Codecs (optional):"
echo "     x264          : ${have_x264}"
echo "     libvpx        : ${have_vpx}"
echo "     libtheora     : ${have_theora}"
echo "     libaom (AV1)  : ${have_aom}"
echo "     SVT-AV1       : ${have_svtav1}"
echo "     dav1d (AV1)   : ${have_dav1d}"
echo "     libwebp       : ${have_webp}"
echo ""
echo "  Subtitle / Text Rendering (optional):"
echo "     libass        : ${have_libass}"
echo "     FreeType      : ${have_freetype}"
echo "     Fontconfig    : ${have_fontconfig}"
echo "     FriBidi       : ${have_fribidi}"
echo "     libzvbi       : ${have_zvbi}"
echo ""
echo "  Security:"
echo "     OpenSSL       : ${have_openssl}"
echo ""
echo "  External Tools:"
echo "     yt-dlp        : ${YTDLP_BIN:-not found}"
echo "     ffmpeg        : ${FFMPEG_BIN:-not found}"
echo "     ffplay        : ${FFPLAY_BIN:-not found}"
echo ""
echo "  Build:"
echo "     ./configure --enable-macos-gui"
echo "     make -C src/DAWCast -j\$(sysctl -n hw.ncpu)"
echo "     make -C src/EditorStudio -j\$(sysctl -n hw.ncpu)"
echo ""
echo "  Binaries:"
echo "     mcaster1dawcast         — Main DAW application"
echo "     dawcast-editor-studio   — Forensic single-file editor"
echo ""
