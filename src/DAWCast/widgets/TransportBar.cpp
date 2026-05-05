// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TransportBar.h"
#include "BevelButton.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QFrame>
#include <QTimer>
#include <QIcon>
#include <QPixmap>

namespace dawcast::widgets {

// Helper: BevelButton hosting an SVG icon. The BevelButton provides the
// button surface; the SVG only draws the symbol. Used for both transport
// hero icons and utility glyphs.
static BevelButton* makeIconCapButton(const QString& svgPath, QSize btnSize,
                                      QSize iconPx, QWidget* parent)
{
    auto* btn = new BevelButton(QIcon(svgPath), QString(), parent);
    btn->setFixedSize(btnSize);
    btn->setIconSize(iconPx);
    return btn;
}

static BevelButton* makeIconGlyphButton(const QString& svgPath, QSize btnSize,
                                        QSize iconPx, QWidget* parent)
{
    auto* btn = new BevelButton(QIcon(svgPath), QString(), parent);
    btn->setFixedSize(btnSize);
    btn->setIconSize(iconPx);
    return btn;
}

// Helper to format sample position as timecode
static QString formatTimecode(int64_t samples, int sampleRate)
{
    if (sampleRate <= 0) sampleRate = 44100;
    double totalSec = static_cast<double>(samples) / sampleRate;
    int hours   = static_cast<int>(totalSec) / 3600;
    int minutes = (static_cast<int>(totalSec) % 3600) / 60;
    int seconds = static_cast<int>(totalSec) % 60;
    int millis  = static_cast<int>((totalSec - static_cast<int>(totalSec)) * 1000);

    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours,   2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(millis,  3, 10, QLatin1Char('0'));
}

// Helper: create a vertical line separator
static QFrame* makeSeparator(QWidget* parent)
{
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setFixedWidth(2);
    return sep;
}

TransportBar::TransportBar(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(4, 2, 4, 2);
    outerLayout->setSpacing(2);

    // ════════════════════════════════════════════════════════════════════
    // Row 1: [Rewind][Play][Stop][REC] | Time | Zoom px/s | BPM [TAP] | [Auto] [XF Auto]
    // ════════════════════════════════════════════════════════════════════

    auto* row1 = new QHBoxLayout;
    row1->setContentsMargins(2, 1, 2, 1);
    row1->setSpacing(4);

    const QSize btnSize(42, 30);
    const QSize smallBtnSize(36, 26);

    // ── Transport Buttons ───────────────────────────────────────────────

    const QSize heroIconPx(20, 20);

    m_rewindBtn = makeIconCapButton(QStringLiteral(":/icons/rewind.svg"),
                                     btnSize, heroIconPx, this);
    m_rewindBtn->setToolTip(tr("Rewind to start (Home) - Returns playhead to position 0:00"));

    m_playBtn = makeIconCapButton(QStringLiteral(":/icons/play.svg"),
                                   btnSize, heroIconPx, this);
    m_playBtn->setCheckable(true);
    m_playBtn->setToolTip(tr("Play (Space) - Start playback from the current playhead position"));

    m_stopBtn = makeIconCapButton(QStringLiteral(":/icons/stop.svg"),
                                   btnSize, heroIconPx, this);
    m_stopBtn->setToolTip(tr("Stop (Space) - Stop playback and reset playhead to start"));

    m_recordBtn = makeIconCapButton(QStringLiteral(":/icons/record.svg"),
                                     btnSize, heroIconPx, this);
    m_recordBtn->setCheckable(true);
    m_recordBtn->setToolTip(tr("Record (R) - Start recording on armed tracks"));

    row1->addWidget(m_rewindBtn);
    row1->addWidget(m_playBtn);
    row1->addWidget(m_stopBtn);
    row1->addWidget(m_recordBtn);

    row1->addWidget(makeSeparator(this));

    // ── Time Display ────────────────────────────────────────────────────

    m_timeDisplay = new QLabel(
        QStringLiteral("00:00:00.000 / 00:00:00.000"), this);
    m_timeDisplay->setAlignment(Qt::AlignCenter);

    QFont monoFont(QStringLiteral("Menlo"));
    if (!monoFont.exactMatch()) {
        monoFont.setFamily(QStringLiteral("Courier New"));
    }
    monoFont.setPointSize(13);
    monoFont.setStyleHint(QFont::Monospace);
    m_timeDisplay->setFont(monoFont);
    m_timeDisplay->setMinimumWidth(280);

    m_timeDisplay->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_timeDisplay->setStyleSheet(
        QStringLiteral("QLabel { background-color: #1a1a1a; color: #00ff88; "
                        "padding: 4px 8px; border-radius: 3px; }"));

    row1->addWidget(m_timeDisplay);

    // ── Selection Display ───────────────────────────────────────────────
    m_selectionLabel = new QLabel(this);
    m_selectionLabel->setAlignment(Qt::AlignCenter);
    m_selectionLabel->setFont(monoFont);
    m_selectionLabel->setMinimumWidth(200);
    m_selectionLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_selectionLabel->setStyleSheet(
        QStringLiteral("QLabel { background-color: #1a1a1a; color: #66aaff; "
                        "padding: 4px 6px; border-radius: 3px; font-size: 10px; }"));
    m_selectionLabel->setToolTip(tr("Time selection range"));
    m_selectionLabel->hide();  // Hidden until a selection is made
    row1->addWidget(m_selectionLabel);

    row1->addWidget(makeSeparator(this));

    // ── Zoom Slider ─────────────────────────────────────────────────────

    auto* zoomIcon = new QLabel(this);
    zoomIcon->setPixmap(QIcon(QStringLiteral(":/icons/magnifier.svg"))
                           .pixmap(16, 16));
    zoomIcon->setFixedWidth(18);
    zoomIcon->setAlignment(Qt::AlignCenter);
    row1->addWidget(zoomIcon);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(10, 1000);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(120);
    m_zoomSlider->setToolTip(tr("Timeline zoom (pixels per second)"));
    row1->addWidget(m_zoomSlider);

    m_zoomLabel = new QLabel(QStringLiteral("100 px/s"), this);
    m_zoomLabel->setFixedWidth(70);
    m_zoomLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_zoomLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #aaaaaa; font-size: 11px; }"));
    row1->addWidget(m_zoomLabel);

    row1->addWidget(makeSeparator(this));

    // ── BPM / Metronome / TAP ───────────────────────────────────────────

    m_metronomeBtn = makeIconGlyphButton(QStringLiteral(":/icons/metronome.svg"),
                                          smallBtnSize, QSize(16, 16), this);
    m_metronomeBtn->setCheckable(true);
    m_metronomeBtn->setToolTip(tr("Metronome - Toggle click track on/off during playback and recording"));
    row1->addWidget(m_metronomeBtn);

    auto* bpmLabel = new QLabel(QStringLiteral("BPM"), this);
    bpmLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #888888; font-size: 10px; }"));
    row1->addWidget(bpmLabel);

    m_tempoSpin = new QDoubleSpinBox(this);
    m_tempoSpin->setRange(20.0, 300.0);
    m_tempoSpin->setValue(120.0);
    m_tempoSpin->setDecimals(1);
    m_tempoSpin->setSingleStep(1.0);
    m_tempoSpin->setFixedWidth(80);
    m_tempoSpin->setToolTip(tr("Tempo (BPM)"));
    row1->addWidget(m_tempoSpin);

    m_tapBtn = new BevelButton(QStringLiteral("TAP"), this);
    m_tapBtn->setFixedSize(QSize(48, 30));
    m_tapBtn->setToolTip(tr("Tap tempo — click repeatedly to set BPM"));
    row1->addWidget(m_tapBtn);

    // Beat indicator — geometric dot (no Unicode, no text) that tints on beat
    m_beatIndicator = new QLabel(this);
    m_beatIndicator->setFixedSize(14, 14);
    m_beatIndicator->setStyleSheet(
        QStringLiteral("QLabel { background-color: #555555; "
                        "border-radius: 7px; margin: 2px; }"));
    m_beatIndicator->setToolTip(tr("Beat indicator"));
    row1->addWidget(m_beatIndicator);

    row1->addWidget(makeSeparator(this));

    // ── Auto / Crossfade ────────────────────────────────────────────────

    m_autoBtn = new BevelButton(QStringLiteral("Auto"), this);
    m_autoBtn->setFixedSize(QSize(50, 30));
    m_autoBtn->setCheckable(true);
    m_autoBtn->setToolTip(tr("Automation write mode"));
    m_autoBtn->setStyleSheet(
        QStringLiteral("QPushButton { font-size: 11px; font-weight: bold; }"));
    row1->addWidget(m_autoBtn);

    m_punchBtn = new BevelButton(QStringLiteral("I/O"), this);
    m_punchBtn->setFixedSize(QSize(40, 30));
    m_punchBtn->setCheckable(true);
    m_punchBtn->setToolTip(tr("Punch In/Out — record only between punch markers"));
    m_punchBtn->setStyleSheet(
        QStringLiteral("QPushButton { font-size: 11px; font-weight: bold; }"));
    row1->addWidget(m_punchBtn);

    m_xfCombo = new QComboBox(this);
    m_xfCombo->addItem(QStringLiteral("XF Auto"));
    m_xfCombo->addItem(QStringLiteral("XF Manual"));
    m_xfCombo->addItem(QStringLiteral("XF Off"));
    m_xfCombo->setFixedWidth(100);
    m_xfCombo->setToolTip(tr("Crossfade behavior when clips overlap"));
    row1->addWidget(m_xfCombo);

    row1->addStretch();

    outerLayout->addLayout(row1);

    // ════════════════════════════════════════════════════════════════════
    // Row 2: [Buses] [Flag] [<] [>] [List] [Grid] | [Loop] [1s zoom preset]
    // ════════════════════════════════════════════════════════════════════

    auto* row2 = new QHBoxLayout;
    row2->setContentsMargins(2, 0, 2, 1);
    row2->setSpacing(4);

    m_busesBtn = new BevelButton(QStringLiteral("Buses"), this);
    m_busesBtn->setFixedSize(QSize(52, 24));
    m_busesBtn->setToolTip(tr("Show audio buses (master, aux sends)"));
    m_busesBtn->setStyleSheet(
        QStringLiteral("QPushButton { font-size: 10px; }"));
    row2->addWidget(m_busesBtn);

    m_rippleBtn = new BevelButton(QStringLiteral("R"), this);
    m_rippleBtn->setFixedSize(QSize(24, 24));
    m_rippleBtn->setCheckable(true);
    m_rippleBtn->setToolTip(tr("Ripple Edit Mode — inserting/deleting shifts subsequent clips"));
    m_rippleBtn->setCheckedFaceColor(QColor(200, 120, 30));
    m_rippleBtn->setStyleSheet(
        QStringLiteral("QPushButton { font-size: 11px; font-weight: bold; }"));
    row2->addWidget(m_rippleBtn);

    m_flagBtn = makeIconGlyphButton(QStringLiteral(":/icons/flag.svg"),
                                     QSize(30, 24), QSize(14, 14), this);
    m_flagBtn->setCheckable(true);
    m_flagBtn->setToolTip(tr("Marker view"));
    row2->addWidget(m_flagBtn);

    m_prevBtn = makeIconGlyphButton(QStringLiteral(":/icons/prev.svg"),
                                     QSize(30, 24), QSize(14, 14), this);
    m_prevBtn->setToolTip(tr("Previous marker / clip boundary"));
    row2->addWidget(m_prevBtn);

    m_nextBtn = makeIconGlyphButton(QStringLiteral(":/icons/next.svg"),
                                     QSize(30, 24), QSize(14, 14), this);
    m_nextBtn->setToolTip(tr("Next marker / clip boundary"));
    row2->addWidget(m_nextBtn);

    m_listBtn = makeIconGlyphButton(QStringLiteral(":/icons/list.svg"),
                                     QSize(30, 24), QSize(14, 14), this);
    m_listBtn->setCheckable(true);
    m_listBtn->setToolTip(tr("List view"));
    row2->addWidget(m_listBtn);

    m_gridBtn = makeIconGlyphButton(QStringLiteral(":/icons/grid.svg"),
                                     QSize(30, 24), QSize(14, 14), this);
    m_gridBtn->setCheckable(true);
    m_gridBtn->setToolTip(tr("Grid view"));
    row2->addWidget(m_gridBtn);

    row2->addWidget(makeSeparator(this));

    // Loop button (moved to row 2 for layout balance)
    m_loopBtn = makeIconGlyphButton(QStringLiteral(":/icons/loop.svg"),
                                     QSize(30, 24), QSize(14, 14), this);
    m_loopBtn->setCheckable(true);
    m_loopBtn->setToolTip(tr("Loop (L) - Repeat playback within the loop region"));
    row2->addWidget(m_loopBtn);

    // Pause button (compact, row 2)
    m_pauseBtn = makeIconCapButton(QStringLiteral(":/icons/pause.svg"),
                                    QSize(30, 24), QSize(14, 14), this);
    m_pauseBtn->setToolTip(tr("Pause - Pause playback and keep the playhead at its current position"));
    row2->addWidget(m_pauseBtn);

    // Fast-forward button
    m_ffBtn = makeIconCapButton(QStringLiteral(":/icons/forward.svg"),
                                 QSize(30, 24), QSize(14, 14), this);
    m_ffBtn->setToolTip(tr("Fast Forward (End) - Jump playhead to the end of the timeline"));
    row2->addWidget(m_ffBtn);

    row2->addWidget(makeSeparator(this));

    // Zoom preset dropdown
    m_zoomPreset = new QComboBox(this);
    m_zoomPreset->addItem(QStringLiteral("1s"));
    m_zoomPreset->addItem(QStringLiteral("5s"));
    m_zoomPreset->addItem(QStringLiteral("10s"));
    m_zoomPreset->addItem(QStringLiteral("30s"));
    m_zoomPreset->addItem(QStringLiteral("1m"));
    m_zoomPreset->addItem(QStringLiteral("5m"));
    m_zoomPreset->setFixedWidth(60);
    m_zoomPreset->setToolTip(tr("Zoom preset — approximate visible window"));
    row2->addWidget(m_zoomPreset);

    // ── Snap-to-Grid selector ──────────────────────────────────────────
    m_snapCombo = new QComboBox(this);
    m_snapCombo->addItem(tr("Snap: Off"),    0);
    m_snapCombo->addItem(tr("Snap: Beat"),   1);
    m_snapCombo->addItem(tr("Snap: Bar"),    2);
    m_snapCombo->addItem(tr("Snap: 1s"),     3);
    m_snapCombo->addItem(tr("Snap: 0.5s"),   4);
    m_snapCombo->addItem(tr("Snap: Frame"),  5);
    m_snapCombo->setFixedWidth(90);
    m_snapCombo->setToolTip(tr("Snap-to-grid mode — quantize clip positions"));
    row2->addWidget(m_snapCombo);

    row2->addStretch();

    outerLayout->addLayout(row2);

    // ── Connections ─────────────────────────────────────────────────────

    // Transport core
    connect(m_rewindBtn, &BevelButton::clicked, this, &TransportBar::rewindClicked);
    connect(m_playBtn,   &BevelButton::clicked, this, &TransportBar::playClicked);
    connect(m_pauseBtn,  &BevelButton::clicked, this, &TransportBar::pauseClicked);
    connect(m_stopBtn,   &BevelButton::clicked, this, &TransportBar::stopClicked);
    connect(m_recordBtn, &BevelButton::clicked, this, &TransportBar::recordClicked);
    connect(m_ffBtn,     &BevelButton::clicked, this, &TransportBar::fastForwardClicked);
    connect(m_loopBtn,       &BevelButton::toggled, this, &TransportBar::loopToggled);
    connect(m_metronomeBtn,  &BevelButton::toggled, this, &TransportBar::metronomeToggled);
    connect(m_tempoSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TransportBar::tempoChanged);

    // Zoom slider -> signal + label
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int val) {
        updateZoomLabel(val);
        emit zoomChanged(val);
    });

    // Zoom presets -> set slider value (approximate px/s for visible window)
    connect(m_zoomPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        // Map preset to approximate px/s
        static const int presets[] = { 1000, 200, 100, 33, 17, 3 };
        if (index >= 0 && index < 6) {
            m_zoomSlider->setValue(presets[index]);
        }
    });

    // TAP tempo
    connect(m_tapBtn, &BevelButton::clicked, this, &TransportBar::onTapTempo);

    // Auto button toggle
    connect(m_autoBtn, &BevelButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_autoBtn->setHighlightColor(QColor(0, 200, 180, 200));  // teal
        } else {
            m_autoBtn->setHighlightColor(QColor(255, 255, 255, 120));
        }
        m_autoBtn->update();
        emit automationWriteToggled(checked);
    });

    // Punch I/O toggle
    connect(m_punchBtn, &BevelButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_punchBtn->setHighlightColor(QColor(255, 120, 40, 200));  // orange
        } else {
            m_punchBtn->setHighlightColor(QColor(255, 255, 255, 120));
        }
        m_punchBtn->update();
        emit punchToggled(checked);
    });

    // Crossfade mode
    connect(m_xfCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TransportBar::crossfadeModeChanged);

    // Secondary row buttons
    connect(m_busesBtn, &BevelButton::clicked, this, &TransportBar::busesClicked);
    connect(m_rippleBtn, &BevelButton::toggled, this, &TransportBar::rippleModeToggled);
    connect(m_prevBtn,  &BevelButton::clicked, this, &TransportBar::prevMarkerClicked);
    connect(m_nextBtn,  &BevelButton::clicked, this, &TransportBar::nextMarkerClicked);
    connect(m_flagBtn,  &BevelButton::toggled, this, &TransportBar::markerViewToggled);
    connect(m_listBtn,  &BevelButton::toggled, this, &TransportBar::listViewToggled);
    connect(m_gridBtn,  &BevelButton::toggled, this, &TransportBar::gridViewToggled);

    // Snap mode selector
    connect(m_snapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        emit snapModeChanged(m_snapCombo->itemData(index).toInt());
    });

    // Metronome button highlight: orange when checked
    connect(m_metronomeBtn, &BevelButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_metronomeBtn->setHighlightColor(QColor(255, 180, 40, 200));
        } else {
            m_metronomeBtn->setHighlightColor(QColor(255, 255, 255, 120));
        }
        m_metronomeBtn->update();
    });

    // Play button highlight: green when checked
    connect(m_playBtn, &BevelButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_playBtn->setHighlightColor(QColor(60, 220, 60, 180));
        } else {
            m_playBtn->setHighlightColor(QColor(255, 255, 255, 120));
        }
        m_playBtn->update();
    });

    // Record button highlight: red when checked
    connect(m_recordBtn, &BevelButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_recordBtn->setHighlightColor(QColor(220, 40, 40, 200));
        } else {
            m_recordBtn->setHighlightColor(QColor(255, 255, 255, 120));
        }
        m_recordBtn->update();
    });

    // Stop resets play and record toggle states
    connect(m_stopBtn, &BevelButton::clicked, this, [this]() {
        m_playBtn->setChecked(false);
        m_recordBtn->setChecked(false);
    });
}

TransportBar::~TransportBar() = default;

void TransportBar::setPlaying(bool playing)
{
    m_playing = playing;
    m_playBtn->setChecked(playing);
}

void TransportBar::setRecording(bool recording)
{
    m_recording = recording;
    m_recordBtn->setChecked(recording);
}

void TransportBar::setPosition(int64_t samples, int sampleRate)
{
    m_position   = samples;
    m_sampleRate = sampleRate > 0 ? sampleRate : 44100;
    updateTimeDisplay();
}

void TransportBar::setDuration(int64_t samples, int sampleRate)
{
    m_duration   = samples;
    m_sampleRate = sampleRate > 0 ? sampleRate : m_sampleRate;
    updateTimeDisplay();
}

void TransportBar::flashBeat(int /*beatNumber*/, bool isDownbeat)
{
    // Flash the beat indicator: bright color for 80ms, then dim
    if (isDownbeat) {
        m_beatIndicator->setStyleSheet(
            QStringLiteral("QLabel { background-color: #ff6600; "
                            "border-radius: 7px; margin: 2px; }"));
    } else {
        m_beatIndicator->setStyleSheet(
            QStringLiteral("QLabel { background-color: #ffcc00; "
                            "border-radius: 7px; margin: 2px; }"));
    }

    // Reset after 80ms
    QTimer::singleShot(80, this, [this]() {
        m_beatIndicator->setStyleSheet(
            QStringLiteral("QLabel { background-color: #555555; "
                            "border-radius: 7px; margin: 2px; }"));
    });
}

void TransportBar::onTapTempo()
{
    if (!m_tapTimer.isValid()) {
        // First tap — just start the timer
        m_tapTimer.start();
        m_tapIntervals.clear();
        return;
    }

    qint64 elapsed = m_tapTimer.elapsed();
    m_tapTimer.restart();

    // Ignore taps that are too far apart (> 3 seconds = < 20 BPM)
    if (elapsed > 3000) {
        m_tapIntervals.clear();
        return;
    }

    m_tapIntervals.append(elapsed);

    // Keep only the last N intervals
    while (m_tapIntervals.size() > kMaxTapSamples) {
        m_tapIntervals.removeFirst();
    }

    // Average the intervals to compute BPM
    if (!m_tapIntervals.isEmpty()) {
        qint64 sum = 0;
        for (qint64 interval : m_tapIntervals) {
            sum += interval;
        }
        double avgMs = static_cast<double>(sum) / m_tapIntervals.size();
        double bpm = 60000.0 / avgMs;

        // Clamp to spinner range
        bpm = qBound(m_tempoSpin->minimum(), bpm, m_tempoSpin->maximum());
        m_tempoSpin->setValue(bpm);
    }
}

void TransportBar::updateTimeDisplay()
{
    QString posStr = formatTimecode(m_position, m_sampleRate);
    QString durStr = formatTimecode(m_duration, m_sampleRate);
    m_timeDisplay->setText(posStr + QStringLiteral(" / ") + durStr);
}

void TransportBar::updateZoomLabel(int value)
{
    m_zoomLabel->setText(QStringLiteral("%1 px/s").arg(value));
}

void TransportBar::setSelection(int64_t startSamples, int64_t endSamples, int sampleRate)
{
    if (startSamples >= endSamples) {
        m_selectionLabel->hide();
        return;
    }
    QString startStr = formatTimecode(startSamples, sampleRate);
    QString endStr   = formatTimecode(endSamples, sampleRate);
    m_selectionLabel->setText(
        QStringLiteral("Sel: %1 - %2").arg(startStr, endStr));
    m_selectionLabel->show();
}

} // namespace dawcast::widgets
