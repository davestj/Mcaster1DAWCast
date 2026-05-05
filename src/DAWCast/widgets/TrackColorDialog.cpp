// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TrackColorDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>
#include <QDialogButtonBox>

namespace dawcast::widgets {

namespace {

// Preset palette: 12 colors matching the DAWCast web version
const QColor kPresetColors[] = {
    QColor(70, 130, 200),    // Blue
    QColor(200, 100, 70),    // Coral
    QColor(80, 170, 100),    // Green
    QColor(180, 140, 60),    // Gold
    QColor(140, 90, 180),    // Purple
    QColor(60, 160, 170),    // Teal
    QColor(220, 80, 80),     // Red
    QColor(100, 180, 220),   // Sky blue
    QColor(180, 120, 180),   // Lavender
    QColor(120, 200, 120),   // Lime
    QColor(220, 160, 80),    // Orange
    QColor(160, 160, 200),   // Slate
};
constexpr int kPresetCount = sizeof(kPresetColors) / sizeof(kPresetColors[0]);
constexpr int kSwatchSize  = 32;

} // anonymous namespace

TrackColorDialog::TrackColorDialog(const QColor& initialColor, QWidget* parent)
    : QDialog(parent)
    , m_selectedColor(initialColor)
{
    setWindowTitle(tr("Track Color"));
    setFixedSize(210, 165);
    buildUI();
}

TrackColorDialog::~TrackColorDialog() = default;

void TrackColorDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Title label
    auto* titleLabel = new QLabel(tr("Choose a track color:"), this);
    titleLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #dde; font-size: 12px; font-weight: bold; }"));
    mainLayout->addWidget(titleLabel);

    // Color grid (4 columns x 3 rows)
    m_grid = new QGridLayout;
    m_grid->setSpacing(4);

    for (int i = 0; i < kPresetCount; ++i) {
        auto* btn = new QPushButton(this);
        btn->setFixedSize(kSwatchSize, kSwatchSize);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(kPresetColors[i].name());
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  border: 2px solid #2a2e3e;"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover {"
            "  border-color: #ffffff;"
            "}")
            .arg(kPresetColors[i].name()));

        int row = i / 4;
        int col = i % 4;
        m_grid->addWidget(btn, row, col);

        QColor color = kPresetColors[i];
        connect(btn, &QPushButton::clicked, this, [this, color]() {
            onPresetClicked(color);
        });
    }

    mainLayout->addLayout(m_grid);

    // Custom color button + Preview
    auto* customRow = new QHBoxLayout;

    auto* customBtn = new QPushButton(tr("Custom..."), this);
    customBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #2e3248; color: #aab;"
        "  border: 1px solid #3a3e55; border-radius: 3px;"
        "  padding: 4px 12px; font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #3a3e58; }"));
    connect(customBtn, &QPushButton::clicked, this, &TrackColorDialog::onCustomColor);
    customRow->addWidget(customBtn);

    customRow->addStretch();

    // Preview swatch
    auto* previewLabel = new QLabel(tr("Preview:"), this);
    previewLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #888; font-size: 10px; }"));
    customRow->addWidget(previewLabel);

    m_previewWidget = new QWidget(this);
    m_previewWidget->setFixedSize(30, 20);
    updatePreview();
    customRow->addWidget(m_previewWidget);

    mainLayout->addLayout(customRow);

    // OK / Cancel buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #2e3248; color: #dde;"
        "  border: 1px solid #3a3e55; border-radius: 3px;"
        "  padding: 4px 16px; font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #3a3e58; }"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // Dialog background
    setStyleSheet(QStringLiteral(
        "TrackColorDialog { background-color: #1e2235; }"));
}

void TrackColorDialog::onPresetClicked(const QColor& color)
{
    m_selectedColor = color;
    updatePreview();
}

void TrackColorDialog::onCustomColor()
{
    QColor color = QColorDialog::getColor(m_selectedColor, this, tr("Custom Color"));
    if (color.isValid()) {
        m_selectedColor = color;
        updatePreview();
    }
}

void TrackColorDialog::updatePreview()
{
    if (m_previewWidget) {
        m_previewWidget->setStyleSheet(QStringLiteral(
            "background-color: %1; border: 1px solid #3a3e55; border-radius: 3px;")
            .arg(m_selectedColor.name()));
    }
}

} // namespace dawcast::widgets
