// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TrackHeaderWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QMenu>
#include <QToolTip>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace dawcast::widgets {

namespace {

// Track header dimensions
constexpr int kTrackHeight   = 80;
constexpr int kBtnSize       = 24;
constexpr int kColorDotSize  = 12;

// Base button style: flat, dark, rounded
const QString kBtnBase = QStringLiteral(
    "QPushButton {"
    "  background-color: #2e3248;"
    "  color: #aab;"
    "  border: 1px solid #3a3e55;"
    "  border-radius: 3px;"
    "  font-size: 10px;"
    "  font-weight: bold;"
    "  padding: 0px;"
    "}"
    "QPushButton:hover {"
    "  background-color: #3a3e58;"
    "  border-color: #505578;"
    "}"
    "QPushButton:pressed {"
    "  background-color: #252840;"
    "}");

} // anonymous namespace

TrackHeaderWidget::TrackHeaderWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(kHeaderWidth);
    setFixedHeight(kTrackHeight);
    setStyleSheet(QStringLiteral(
        "TrackHeaderWidget { background-color: #1e2235; border-bottom: 1px solid #2a2e3e; }"));

    buildUI();
}

TrackHeaderWidget::~TrackHeaderWidget() = default;

void TrackHeaderWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(2);

    // ── Row 1: Grip + Color dot + Track name + Dropdown ────────────────
    auto* row1 = new QHBoxLayout;
    row1->setSpacing(4);
    row1->setContentsMargins(0, 0, 0, 0);

    // Drag handle (grip dots) for track reorder
    auto* gripLabel = new QLabel(QStringLiteral("::"), this);  // grip dots
    gripLabel->setFixedWidth(14);
    gripLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #556; font-size: 10px; padding: 0; }"));
    gripLabel->setToolTip(tr("Drag to reorder"));
    gripLabel->setCursor(Qt::OpenHandCursor);
    row1->addWidget(gripLabel);

    // Color indicator square
    m_colorDot = new QWidget(this);
    m_colorDot->setFixedSize(kColorDotSize, kColorDotSize);
    m_colorDot->setStyleSheet(
        QStringLiteral("background-color: %1; border-radius: 2px;").arg(m_trackColor.name()));
    row1->addWidget(m_colorDot);

    // Freeze indicator (snowflake) — hidden by default
    m_freezeLabel = new QLabel(QStringLiteral("*"), this);  // frozen indicator
    m_freezeLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #78b4ff; font-size: 12px; padding: 0; }"));
    m_freezeLabel->setToolTip(tr("Frozen"));
    m_freezeLabel->setFixedWidth(16);
    m_freezeLabel->setVisible(false);
    row1->addWidget(m_freezeLabel);

    // Editable track name
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Track Name"));
    m_nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: transparent; color: #dde; border: none;"
        "  font-size: 11px; font-weight: bold; padding: 1px 2px;"
        "}"
        "QLineEdit:focus {"
        "  background: #252840; border: 1px solid #4a4e68; border-radius: 2px;"
        "}"));
    row1->addWidget(m_nameEdit, 1);

    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_trackName = text;
        emit trackNameEdited(text);
    });

    // Dropdown menu button
    m_menuBtn = new QPushButton(QStringLiteral("v"), this);  // dropdown
    m_menuBtn->setFixedSize(20, 18);
    m_menuBtn->setStyleSheet(kBtnBase +
        QStringLiteral(" QPushButton { font-size: 8px; padding: 0; }"));
    row1->addWidget(m_menuBtn);

    // Vertical waveform zoom indicator — hidden at 1.0x
    m_zoomLabel = new QLabel(this);
    m_zoomLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #99a; font-size: 8px; padding: 0 2px; }"));
    m_zoomLabel->setFixedWidth(30);
    m_zoomLabel->setVisible(false);
    row1->addWidget(m_zoomLabel);

    connect(m_menuBtn, &QPushButton::clicked, this, &TrackHeaderWidget::showTrackMenu);

    mainLayout->addLayout(row1);

    // ── Row 2: 7 small toggle buttons ─────────────────────────────────
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(2);
    row2->setContentsMargins(0, 0, 0, 0);

    auto makeBtn = [this](const QString& text) -> QPushButton* {
        auto* btn = new QPushButton(text, this);
        btn->setFixedSize(kBtnSize, kBtnSize);
        btn->setCheckable(true);
        btn->setStyleSheet(kBtnBase);
        return btn;
    };

    // M (mute)
    m_muteBtn = makeBtn(tr("M"));
    row2->addWidget(m_muteBtn);
    connect(m_muteBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_muted = checked;
        updateButtonStyle(m_muteBtn, checked, QColor(0xc0, 0x78, 0x20));  // orange
        emit muteToggled(checked);
    });

    // S (solo)
    m_soloBtn = makeBtn(tr("S"));
    row2->addWidget(m_soloBtn);
    connect(m_soloBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_solo = checked;
        updateButtonStyle(m_soloBtn, checked, QColor(0xc0, 0xb8, 0x30));  // yellow
        emit soloToggled(checked);
    });

    // Record arm (red circle)
    m_recBtn = makeBtn(QStringLiteral("R"));
    row2->addWidget(m_recBtn);
    connect(m_recBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_recordArmed = checked;
        updateButtonStyle(m_recBtn, checked, QColor(0xc0, 0x30, 0x30));  // red
        emit recordArmToggled(checked);
    });

    // Monitor (headphone icon) — input monitoring toggle
    m_monitorBtn = makeBtn(QStringLiteral("MON"));
    m_monitorBtn->setToolTip(tr("Input Monitor — hear your input in real-time"));
    row2->addWidget(m_monitorBtn);
    connect(m_monitorBtn, &QPushButton::toggled, this, [this](bool checked) {
        updateButtonStyle(m_monitorBtn, checked, QColor(0x30, 0xa0, 0xc0));  // teal
        emit inputMonitorToggled(checked);
    });

    // EQ
    m_eqBtn = makeBtn(QStringLiteral("EQ"));
    row2->addWidget(m_eqBtn);
    connect(m_eqBtn, &QPushButton::clicked, this, [this]() {
        emit eqRequested(m_trackIndex);
    });
    m_eqBtn->setCheckable(false);  // EQ opens a dialog, no toggle state

    // Scissors (split tool)
    m_splitBtn = makeBtn(QStringLiteral("CUT"));
    row2->addWidget(m_splitBtn);
    connect(m_splitBtn, &QPushButton::toggled, this, [this](bool checked) {
        updateButtonStyle(m_splitBtn, checked, QColor(0x50, 0x90, 0xd0));  // blue
        emit splitToolToggled(checked);
    });

    // Automation (tilde / wave)
    m_autoBtn = makeBtn(QStringLiteral("~"));
    row2->addWidget(m_autoBtn);
    connect(m_autoBtn, &QPushButton::toggled, this, [this](bool checked) {
        updateButtonStyle(m_autoBtn, checked, QColor(0x40, 0xb0, 0x80));  // green
        emit automationToggled(checked);
    });

    // Settings (gear)
    m_settingsBtn = makeBtn(QStringLiteral("SET"));
    row2->addWidget(m_settingsBtn);
    connect(m_settingsBtn, &QPushButton::clicked, this, [this]() {
        emit settingsRequested(m_trackIndex);
    });
    m_settingsBtn->setCheckable(false);

    row2->addStretch();
    mainLayout->addLayout(row2);

    // ── Row 3: Volume slider ──────────────────────────────────────────
    auto* row3 = new QHBoxLayout;
    row3->setSpacing(4);
    row3->setContentsMargins(0, 0, 0, 0);

    auto* volLabel = new QLabel(tr("Vol"), this);
    volLabel->setStyleSheet(QStringLiteral("QLabel { color: #778; font-size: 9px; }"));
    volLabel->setFixedWidth(20);
    row3->addWidget(volLabel);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(-600, 60);  // -60.0 dB to +6.0 dB in tenths
    m_volumeSlider->setValue(0);          // 0 dB default
    m_volumeSlider->setStyleSheet(sliderStyleSheet(QColor(0x50, 0x90, 0xd0)));
    row3->addWidget(m_volumeSlider, 1);

    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_volume = static_cast<float>(value) / 10.0f;
        emit volumeChanged(m_volume);
        // Show tooltip with current dB value
        QString tip = (m_volume > 0.0f)
            ? QStringLiteral("+%1 dB").arg(m_volume, 0, 'f', 1)
            : QStringLiteral("%1 dB").arg(m_volume, 0, 'f', 1);
        QToolTip::showText(QCursor::pos(), tip, m_volumeSlider);
    });

    mainLayout->addLayout(row3);

    // ── Row 4: Pan slider with L / R labels ───────────────────────────
    auto* row4 = new QHBoxLayout;
    row4->setSpacing(4);
    row4->setContentsMargins(0, 0, 0, 0);

    auto* panLabelL = new QLabel(tr("L"), this);
    panLabelL->setStyleSheet(QStringLiteral("QLabel { color: #778; font-size: 9px; }"));
    panLabelL->setFixedWidth(10);
    row4->addWidget(panLabelL);

    m_panSlider = new QSlider(Qt::Horizontal, this);
    m_panSlider->setRange(-100, 100);
    m_panSlider->setValue(0);   // center
    m_panSlider->setStyleSheet(sliderStyleSheet(QColor(0x40, 0xb0, 0x80)));
    row4->addWidget(m_panSlider, 1);

    auto* panLabelR = new QLabel(tr("R"), this);
    panLabelR->setStyleSheet(QStringLiteral("QLabel { color: #778; font-size: 9px; }"));
    panLabelR->setFixedWidth(10);
    row4->addWidget(panLabelR);

    connect(m_panSlider, &QSlider::valueChanged, this, [this](int value) {
        m_pan = static_cast<float>(value);
        emit panChanged(m_pan);
        QString tip;
        if (value == 0) tip = tr("Center");
        else if (value < 0) tip = QStringLiteral("L %1").arg(-value);
        else tip = QStringLiteral("R %1").arg(value);
        QToolTip::showText(QCursor::pos(), tip, m_panSlider);
    });

    mainLayout->addLayout(row4);
}

void TrackHeaderWidget::updateButtonStyle(QPushButton* btn, bool active,
                                          const QColor& activeColor) const
{
    if (active) {
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1; color: white;"
            "  border: 1px solid %2; border-radius: 3px;"
            "  font-size: 10px; font-weight: bold; padding: 0px;"
            "}"
            "QPushButton:hover { background-color: %3; }")
            .arg(activeColor.name(),
                 activeColor.lighter(130).name(),
                 activeColor.lighter(115).name()));
    } else {
        btn->setStyleSheet(kBtnBase);
    }
}

QString TrackHeaderWidget::sliderStyleSheet(const QColor& accentColor) const
{
    return QStringLiteral(
        "QSlider::groove:horizontal {"
        "  background: #252840; height: 4px; border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: %1; width: 10px; height: 10px;"
        "  margin: -3px 0; border-radius: 5px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  background: %2;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: %3; border-radius: 2px;"
        "}")
        .arg(accentColor.name(),
             accentColor.lighter(120).name(),
             accentColor.darker(140).name());
}

void TrackHeaderWidget::showTrackMenu()
{
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #1e2235; color: #dde; border: 1px solid #3a3e55; }"
        "QMenu::item:selected { background: #3a3e58; }"));

    menu.addAction(tr("Rename Track"), this, [this]() {
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
    });
    menu.addAction(tr("Change Color..."), this, [this]() {
        emit colorChangeRequested(m_trackIndex);
    });
    menu.addSeparator();
    menu.addAction(tr("Duplicate Track"), this, [this]() {
        emit duplicateRequested(m_trackIndex);
    });
    menu.addAction(tr("Delete Track"), this, [this]() {
        emit deleteRequested(m_trackIndex);
    });
    menu.addSeparator();
    if (m_frozen) {
        menu.addAction(tr("Unfreeze Track"), this, [this]() {
            emit freezeRequested(m_trackIndex);
        });
    } else {
        menu.addAction(tr("Freeze Track"), this, [this]() {
            emit freezeRequested(m_trackIndex);
        });
    }
    menu.addAction(tr("Bounce to Audio"), this, [this]() {
        emit bounceRequested(m_trackIndex);
    });
    menu.addSeparator();
    menu.addAction(tr("Create Group from Track"), this, [this]() {
        emit createGroupRequested(m_trackIndex);
    });

    menu.addSeparator();

    // ── Track Presets ──────────────────────────────────────────────
    menu.addAction(tr("Save Track Preset..."), this, [this]() {
        emit saveTrackPresetRequested(m_trackIndex);
    });

    auto* loadPresetMenu = menu.addMenu(tr("Load Track Preset"));

    // Factory presets
    static const QStringList factoryPresets = {
        QStringLiteral("Podcast Voice"),
        QStringLiteral("Music Instrument"),
        QStringLiteral("Broadcast Voice"),
        QStringLiteral("Sound Design")
    };
    for (const QString& name : factoryPresets) {
        loadPresetMenu->addAction(name, this, [this, name]() {
            emit loadTrackPresetRequested(m_trackIndex, name);
        });
    }

    // User presets from ~/.mcaster1/track_presets/
    QDir presetDir(QDir::homePath() + QStringLiteral("/.mcaster1/track_presets"));
    if (presetDir.exists()) {
        QStringList yamlFiles = presetDir.entryList(
            {QStringLiteral("*.yaml"), QStringLiteral("*.yml")},
            QDir::Files, QDir::Name);
        if (!yamlFiles.isEmpty()) {
            loadPresetMenu->addSeparator();
            for (const QString& file : yamlFiles) {
                QString presetName = QFileInfo(file).completeBaseName();
                // Skip factory names (already listed above)
                if (factoryPresets.contains(presetName, Qt::CaseInsensitive))
                    continue;
                loadPresetMenu->addAction(presetName, this, [this, presetName]() {
                    emit loadTrackPresetRequested(m_trackIndex, presetName);
                });
            }
        }
    }

    menu.exec(m_menuBtn->mapToGlobal(QPoint(0, m_menuBtn->height())));
}

// ── Public setters / getters ──────────────────────────────────────────────

void TrackHeaderWidget::setTrackName(const QString& name)
{
    m_trackName = name;
    if (m_nameEdit && m_nameEdit->text() != name)
        m_nameEdit->setText(name);
}

void TrackHeaderWidget::setTrackColor(const QColor& color)
{
    m_trackColor = color;
    if (m_colorDot)
        m_colorDot->setStyleSheet(
            QStringLiteral("background-color: %1; border-radius: 2px;").arg(color.name()));
}

void TrackHeaderWidget::setTrackIndex(int index)
{
    m_trackIndex = index;
}

void TrackHeaderWidget::setRecordArmed(bool armed)
{
    m_recordArmed = armed;
    if (m_recBtn && m_recBtn->isChecked() != armed)
        m_recBtn->setChecked(armed);
}

void TrackHeaderWidget::setMuted(bool muted)
{
    m_muted = muted;
    if (m_muteBtn && m_muteBtn->isChecked() != muted)
        m_muteBtn->setChecked(muted);
}

void TrackHeaderWidget::setSolo(bool solo)
{
    m_solo = solo;
    if (m_soloBtn && m_soloBtn->isChecked() != solo)
        m_soloBtn->setChecked(solo);
}

void TrackHeaderWidget::setVerticalZoom(float zoom)
{
    m_verticalZoom = std::clamp(zoom, 0.25f, 4.0f);
    if (m_zoomLabel) {
        // Show the label only when zoom differs from 1.0x
        bool showZoom = (m_verticalZoom < 0.99f || m_verticalZoom > 1.01f);
        m_zoomLabel->setVisible(showZoom);
        if (showZoom) {
            m_zoomLabel->setText(QStringLiteral("%1x").arg(
                static_cast<double>(m_verticalZoom), 0, 'f', 1));
        }
    }
}

void TrackHeaderWidget::setFrozen(bool frozen)
{
    m_frozen = frozen;

    // Show/hide the snowflake indicator
    if (m_freezeLabel)
        m_freezeLabel->setVisible(frozen);

    // Dim the EQ and settings buttons when frozen (effects are bypassed)
    if (m_eqBtn)
        m_eqBtn->setEnabled(!frozen);
    if (m_settingsBtn)
        m_settingsBtn->setEnabled(!frozen);
}

QString TrackHeaderWidget::trackName() const { return m_trackName; }
QColor  TrackHeaderWidget::trackColor() const { return m_trackColor; }
int     TrackHeaderWidget::trackIndex() const { return m_trackIndex; }
bool    TrackHeaderWidget::isRecordArmed() const { return m_recordArmed; }
bool    TrackHeaderWidget::isMuted() const { return m_muted; }
bool    TrackHeaderWidget::isSolo() const { return m_solo; }
float   TrackHeaderWidget::volume() const { return m_volume; }
float   TrackHeaderWidget::pan() const { return m_pan; }
float   TrackHeaderWidget::verticalZoom() const { return m_verticalZoom; }
bool    TrackHeaderWidget::isFrozen() const { return m_frozen; }

} // namespace dawcast::widgets
