// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AboutDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QApplication>
#include <QSysInfo>
#include <QFrame>
#include <QFont>
#include <QPainter>
#ifdef HAVE_QT6SVG
#include <QSvgRenderer>
#endif
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>

extern "C" {
#include <libavformat/version.h>
#include <libavutil/version.h>
}
#include <portaudio.h>

#ifdef HAVE_TAGLIB
#include <taglib/taglib.h>
#endif

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

namespace dawcast::widgets {

// Dark themed palette colors
static const QColor kBgColor(0x1a, 0x1a, 0x2e);
static const QColor kTextColor(0xe0, 0xe0, 0xe8);
static const QColor kDimText(0x90, 0x90, 0xa0);
static const QColor kTealAccent(0x4e, 0xcd, 0xc4);
static const QColor kSeparator(0x30, 0x30, 0x48);

/// Helper: create a styled label.
static QLabel* makeLabel(const QString& text, int pixelSize, bool bold,
                         const QColor& color, Qt::Alignment align,
                         QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setAlignment(align);
    QFont f = lbl->font();
    f.setPixelSize(pixelSize);
    f.setBold(bold);
    lbl->setFont(f);

    QPalette pal = lbl->palette();
    pal.setColor(QPalette::WindowText, color);
    lbl->setPalette(pal);

    return lbl;
}

/// Helper: create a horizontal separator line.
static QFrame* makeSeparator(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);

    QPalette pal = line->palette();
    pal.setColor(QPalette::Dark, kSeparator);
    pal.setColor(QPalette::Light, kSeparator);
    line->setPalette(pal);

    return line;
}

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Mcaster1DAWCast"));
    setFixedSize(480, 540);

    // Dark background
    QPalette dlgPal = palette();
    dlgPal.setColor(QPalette::Window, kBgColor);
    dlgPal.setColor(QPalette::WindowText, kTextColor);
    dlgPal.setColor(QPalette::Base, kBgColor);
    dlgPal.setColor(QPalette::Button, QColor(0x28, 0x28, 0x40));
    dlgPal.setColor(QPalette::ButtonText, kTextColor);
    setPalette(dlgPal);
    setAutoFillBackground(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(24, 20, 24, 16);

    // -- App icon (128x128, rendered from SVG) --
    auto* iconLabel = new QLabel(this);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedHeight(140);

    QPixmap iconPix(128, 128);
    iconPix.fill(Qt::transparent);
    {
        QPainter iconPainter(&iconPix);
        iconPainter.setRenderHint(QPainter::Antialiasing, true);
        bool svgOk = false;

#ifdef HAVE_QT6SVG
        QSvgRenderer svg(QStringLiteral(":/icons/icons/podcast.svg"));
        if (svg.isValid()) {
            svg.render(&iconPainter, QRectF(0, 0, 128, 128));
            svgOk = true;
        }
#endif

        if (!svgOk) {
            // Fallback: draw a circle with the app initial
            iconPainter.setPen(Qt::NoPen);
            iconPainter.setBrush(kTealAccent);
            iconPainter.drawEllipse(24, 24, 80, 80);
            QFont iconFont;
            iconFont.setPixelSize(48);
            iconFont.setBold(true);
            iconPainter.setFont(iconFont);
            iconPainter.setPen(kBgColor);
            iconPainter.drawText(QRect(0, 0, 128, 128), Qt::AlignCenter,
                                 QStringLiteral("M"));
        }
    }
    iconLabel->setPixmap(iconPix);
    layout->addWidget(iconLabel);

    // -- Title --
    layout->addWidget(
        makeLabel(QStringLiteral("Mcaster1DAWCast"),
                  24, true, kTextColor, Qt::AlignCenter, this));

    // -- Version --
    layout->addWidget(
        makeLabel(QStringLiteral("Version 1.0.0-alpha"),
                  13, false, kDimText, Qt::AlignCenter, this));

    // -- Tagline --
    layout->addWidget(
        makeLabel(tr("Multi-Channel DAW for Broadcasting, Webcasting,\n"
                     "Podcasting & Video Editing"),
                  12, false, kTealAccent, Qt::AlignCenter, this));

    layout->addSpacing(4);
    layout->addWidget(makeSeparator(this));
    layout->addSpacing(4);

    // -- Build info --
    QString buildInfo = QStringLiteral(
        "Qt %1  |  %2  |  %3\nBuilt: %4")
        .arg(QString::fromUtf8(qVersion()),
             QStringLiteral(
#if defined(__clang__)
                 "Clang " __clang_version__
#elif defined(__GNUC__)
                 "GCC " __VERSION__
#elif defined(_MSC_VER)
                 "MSVC " + QString::number(_MSC_VER)
#else
                 "Unknown compiler"
#endif
             ),
             QSysInfo::currentCpuArchitecture(),
             QStringLiteral(__DATE__ " " __TIME__));

    auto* buildLabel = makeLabel(buildInfo, 11, false, kDimText,
                                 Qt::AlignCenter, this);
    buildLabel->setWordWrap(true);
    layout->addWidget(buildLabel);

    layout->addSpacing(4);
    layout->addWidget(makeSeparator(this));
    layout->addSpacing(4);

    // -- Library versions --
    QStringList libs;

    libs << QStringLiteral("FFmpeg libavformat %1.%2.%3")
                .arg(LIBAVFORMAT_VERSION_MAJOR)
                .arg(LIBAVFORMAT_VERSION_MINOR)
                .arg(LIBAVFORMAT_VERSION_MICRO);

    libs << QStringLiteral("PortAudio %1")
                .arg(QString::fromUtf8(Pa_GetVersionText()));

#ifdef HAVE_TAGLIB
    libs << QStringLiteral("TagLib %1.%2.%3")
                .arg(TAGLIB_MAJOR_VERSION)
                .arg(TAGLIB_MINOR_VERSION)
                .arg(TAGLIB_PATCH_VERSION);
#endif

#ifdef HAVE_SQLITE3
    libs << QStringLiteral("SQLite %1")
                .arg(QString::fromUtf8(sqlite3_libversion()));
#endif

    auto* libsLabel = makeLabel(libs.join(QStringLiteral("  |  ")),
                                11, false, kDimText, Qt::AlignCenter, this);
    libsLabel->setWordWrap(true);
    layout->addWidget(libsLabel);

    layout->addSpacing(4);
    layout->addWidget(makeSeparator(this));
    layout->addSpacing(4);

    // -- Credits --
    auto* creditsLabel = makeLabel(
        QStringLiteral("David St. John  <davestj@gmail.com>"),
        12, false, kTextColor, Qt::AlignCenter, this);
    layout->addWidget(creditsLabel);

    // -- License --
    auto* licenseLabel = makeLabel(
        QStringLiteral("GNU General Public License v2.0 or later"),
        11, false, kDimText, Qt::AlignCenter, this);
    layout->addWidget(licenseLabel);

    // -- Link --
    auto* linkLabel = new QLabel(
        QStringLiteral("<a href=\"https://mcaster1.com\" "
                       "style=\"color: #4ecdc4; text-decoration: none;\">"
                       "mcaster1.com</a>"),
        this);
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setTextFormat(Qt::RichText);
    linkLabel->setOpenExternalLinks(true);
    linkLabel->setFont([]{
        QFont f;
        f.setPixelSize(12);
        return f;
    }());
    layout->addWidget(linkLabel);

    layout->addStretch();

    // -- OK button --
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);

    // Style the button to match the dark theme
    QPalette btnPal = buttonBox->palette();
    btnPal.setColor(QPalette::Button, QColor(0x35, 0x35, 0x55));
    btnPal.setColor(QPalette::ButtonText, kTextColor);
    buttonBox->setPalette(btnPal);

    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

AboutDialog::~AboutDialog() = default;

} // namespace dawcast::widgets
