// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AboutDialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QApplication>

extern "C" {
#include <libavutil/version.h>
}
#include <portaudio.h>

namespace dawcast::widgets {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Mcaster1DAWCast"));
    setFixedSize(420, 360);

    auto* layout = new QVBoxLayout(this);

    // Icon placeholder
    auto* iconLabel = new QLabel(this);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setText(QStringLiteral("[App Icon]"));
    iconLabel->setFixedHeight(64);
    layout->addWidget(iconLabel);

    // App name and version
    auto* nameLabel = new QLabel(QStringLiteral("<h2>Mcaster1DAWCast</h2>"), this);
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    auto* versionLabel = new QLabel(QStringLiteral("Version 0.1.0-dev"), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    // Copyright
    auto* copyrightLabel = new QLabel(
        QStringLiteral("Copyright (C) 2026 David St. John\n"
                       "Licensed under GPL-2.0-or-later"), this);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyrightLabel);

    // Library versions
    QString libs = QStringLiteral(
        "Qt %1\n"
        "FFmpeg libavutil %2.%3.%4\n"
        "PortAudio %5")
        .arg(qVersion())
        .arg(LIBAVUTIL_VERSION_MAJOR)
        .arg(LIBAVUTIL_VERSION_MINOR)
        .arg(LIBAVUTIL_VERSION_MICRO)
        .arg(QString::fromUtf8(Pa_GetVersionText()));

    auto* libsLabel = new QLabel(libs, this);
    libsLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(libsLabel);

    layout->addStretch();

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

AboutDialog::~AboutDialog() = default;

} // namespace dawcast::widgets
