// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ParametricEQDialog.h"
#include "EmbossedKnob.h"
#include "ParametricEQ.h"
#include "Biquad.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QFontMetrics>

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dawcast::widgets {

// ── Band colors ───────────────────────────────────────────────────────────

const QColor ParametricEQDialog::kBandColors[NumBands] = {
    QColor(220,  50,  50),   // Band 1  — red
    QColor(240, 130,  30),   // Band 2  — orange
    QColor(230, 200,  30),   // Band 3  — yellow
    QColor(100, 200,  50),   // Band 4  — lime
    QColor( 30, 180, 100),   // Band 5  — green
    QColor( 30, 180, 200),   // Band 6  — teal
    QColor( 50, 120, 220),   // Band 7  — blue
    QColor(100,  70, 220),   // Band 8  — indigo
    QColor(170,  60, 200),   // Band 9  — purple
    QColor(220,  60, 160),   // Band 10 — magenta
};

// ── Constructor / Destructor ──────────────────────────────────────────────

ParametricEQDialog::ParametricEQDialog(ParametricEQ* eq, QWidget* parent)
    : QDialog(parent)
    , m_eq(eq)
{
    setWindowTitle(tr("Parametric EQ"));
    setMinimumSize(kCurveLeft + kCurveWidth + 50, kControlsTop + 200);
    resize(kCurveLeft + kCurveWidth + 50, kControlsTop + 200);
    setMouseTracking(true);

    // Read current band state from the ParametricEQ model
    if (m_eq) {
        for (int b = 0; b < NumBands; ++b) {
            m_bands[b].freq   = m_eq->parameter(b * ParametricEQ::ParamsPerBand + 0);
            m_bands[b].q      = m_eq->parameter(b * ParametricEQ::ParamsPerBand + 1);
            m_bands[b].gainDb = m_eq->parameter(b * ParametricEQ::ParamsPerBand + 2);
            m_bands[b].type   = static_cast<int>(m_eq->parameter(b * ParametricEQ::ParamsPerBand + 3));
            m_bands[b].enabled = true;
        }
    }

    // ── Band control strip (below the curve area) ─────────────────────────

    auto* controlsWidget = new QWidget(this);
    controlsWidget->setGeometry(kCurveLeft, kControlsTop,
                                kCurveWidth, 170);

    auto* controlsGrid = new QGridLayout(controlsWidget);
    controlsGrid->setContentsMargins(0, 0, 0, 0);
    controlsGrid->setSpacing(2);

    const char* typeNames[] = { "LS", "PK", "HS", "HP", "LP" };

    for (int b = 0; b < NumBands; ++b) {
        // Column header: band number with colored background
        auto* header = new QLabel(QString::number(b + 1), controlsWidget);
        header->setAlignment(Qt::AlignCenter);
        header->setFixedHeight(18);
        QPalette pal = header->palette();
        pal.setColor(QPalette::Window, kBandColors[b].darker(140));
        pal.setColor(QPalette::WindowText, Qt::white);
        header->setAutoFillBackground(true);
        header->setPalette(pal);
        QFont hdrFont = header->font();
        hdrFont.setPointSize(8);
        hdrFont.setBold(true);
        header->setFont(hdrFont);
        controlsGrid->addWidget(header, 0, b, Qt::AlignCenter);

        // Type combo
        auto* typeCombo = new QComboBox(controlsWidget);
        for (const char* tn : typeNames)
            typeCombo->addItem(QString::fromLatin1(tn));
        typeCombo->setCurrentIndex(m_bands[b].type);
        typeCombo->setFixedWidth(50);
        QFont comboFont = typeCombo->font();
        comboFont.setPointSize(9);
        typeCombo->setFont(comboFont);
        controlsGrid->addWidget(typeCombo, 1, b, Qt::AlignCenter);
        m_widgets[b].typeCombo = typeCombo;

        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, b](int idx) {
            m_bands[b].type = idx;
            pushBandToEQ(b);
            update();
        });

        // Frequency knob
        auto* freqKnob = new EmbossedKnob(controlsWidget);
        freqKnob->setRange(kMinFreq, kMaxFreq);
        freqKnob->setValue(m_bands[b].freq);
        freqKnob->setLabel(tr("Freq"));
        freqKnob->setSuffix(QStringLiteral(" Hz"));
        freqKnob->setDecimals(0);
        freqKnob->setKnobSize(32);
        freqKnob->setArcColor(kBandColors[b]);
        controlsGrid->addWidget(freqKnob, 2, b, Qt::AlignCenter);
        m_widgets[b].freqKnob = freqKnob;

        connect(freqKnob, &EmbossedKnob::valueChanged, this, [this, b](float val) {
            m_bands[b].freq = val;
            pushBandToEQ(b);
            update();
        });

        // Gain knob
        auto* gainKnob = new EmbossedKnob(controlsWidget);
        gainKnob->setRange(kMinDb, kMaxDb);
        gainKnob->setValue(m_bands[b].gainDb);
        gainKnob->setLabel(tr("Gain"));
        gainKnob->setSuffix(QStringLiteral(" dB"));
        gainKnob->setDecimals(1);
        gainKnob->setKnobSize(32);
        gainKnob->setArcColor(kBandColors[b]);
        controlsGrid->addWidget(gainKnob, 3, b, Qt::AlignCenter);
        m_widgets[b].gainKnob = gainKnob;

        connect(gainKnob, &EmbossedKnob::valueChanged, this, [this, b](float val) {
            m_bands[b].gainDb = val;
            pushBandToEQ(b);
            update();
        });

        // Q knob
        auto* qKnob = new EmbossedKnob(controlsWidget);
        qKnob->setRange(0.1f, 20.0f);
        qKnob->setValue(m_bands[b].q);
        qKnob->setLabel(tr("Q"));
        qKnob->setSuffix(QString());
        qKnob->setDecimals(2);
        qKnob->setKnobSize(32);
        qKnob->setArcColor(kBandColors[b]);
        controlsGrid->addWidget(qKnob, 4, b, Qt::AlignCenter);
        m_widgets[b].qKnob = qKnob;

        connect(qKnob, &EmbossedKnob::valueChanged, this, [this, b](float val) {
            m_bands[b].q = val;
            pushBandToEQ(b);
            update();
        });

        // Enable checkbox
        auto* enableCB = new QCheckBox(controlsWidget);
        enableCB->setChecked(m_bands[b].enabled);
        enableCB->setToolTip(tr("Enable/disable band %1").arg(b + 1));
        controlsGrid->addWidget(enableCB, 5, b, Qt::AlignCenter);
        m_widgets[b].enableCB = enableCB;

        connect(enableCB, &QCheckBox::toggled, this, [this, b](bool checked) {
            m_bands[b].enabled = checked;
            // When disabled, set gain to 0 in the EQ; when re-enabled, restore
            pushBandToEQ(b);
            update();
        });
    }
}

ParametricEQDialog::~ParametricEQDialog() = default;

// ── Coordinate Mapping ────────────────────────────────────────────────────

QRect ParametricEQDialog::curveRect() const
{
    return QRect(kCurveLeft, kCurveTop, kCurveWidth, kCurveHeight);
}

int ParametricEQDialog::freqToX(float freq) const
{
    // Log-scale mapping from [kMinFreq, kMaxFreq] to [kCurveLeft, kCurveLeft+kCurveWidth]
    float logMin = std::log10(kMinFreq);
    float logMax = std::log10(kMaxFreq);
    float logFreq = std::log10(std::clamp(freq, kMinFreq, kMaxFreq));
    float frac = (logFreq - logMin) / (logMax - logMin);
    return kCurveLeft + static_cast<int>(frac * kCurveWidth);
}

float ParametricEQDialog::xToFreq(int x) const
{
    float frac = static_cast<float>(x - kCurveLeft) / kCurveWidth;
    frac = std::clamp(frac, 0.0f, 1.0f);
    float logMin = std::log10(kMinFreq);
    float logMax = std::log10(kMaxFreq);
    return std::pow(10.0f, logMin + frac * (logMax - logMin));
}

int ParametricEQDialog::dbToY(float db) const
{
    // Linear mapping from [kMaxDb, kMinDb] to [kCurveTop, kCurveTop+kCurveHeight]
    // (kMaxDb at top, kMinDb at bottom)
    float frac = (kMaxDb - db) / (kMaxDb - kMinDb);
    return kCurveTop + static_cast<int>(frac * kCurveHeight);
}

float ParametricEQDialog::yToDb(int y) const
{
    float frac = static_cast<float>(y - kCurveTop) / kCurveHeight;
    frac = std::clamp(frac, 0.0f, 1.0f);
    return kMaxDb - frac * (kMaxDb - kMinDb);
}

// ── Biquad Magnitude Response ─────────────────────────────────────────────

float ParametricEQDialog::bandMagnitudeDb(int band, float freq) const
{
    if (band < 0 || band >= NumBands) return 0.0f;
    if (!m_bands[band].enabled) return 0.0f;

    const auto& b = m_bands[band];

    // Compute biquad coefficients for this band
    BiquadCoeffs c;
    switch (static_cast<ParametricEQ::BandType>(b.type)) {
    case ParametricEQ::BandType::LowShelf:
        c = Biquad::lowshelf(b.freq, b.q, b.gainDb, m_sampleRate);
        break;
    case ParametricEQ::BandType::Peaking:
        c = Biquad::peaking(b.freq, b.q, b.gainDb, m_sampleRate);
        break;
    case ParametricEQ::BandType::HighShelf:
        c = Biquad::highshelf(b.freq, b.q, b.gainDb, m_sampleRate);
        break;
    case ParametricEQ::BandType::HighPass:
        c = Biquad::highpass(b.freq, b.q, m_sampleRate);
        break;
    case ParametricEQ::BandType::LowPass:
        c = Biquad::lowpass(b.freq, b.q, m_sampleRate);
        break;
    }

    // Evaluate H(z) magnitude at the given frequency
    double w = 2.0 * M_PI * freq / m_sampleRate;
    double cosW  = std::cos(w);
    double cos2W = std::cos(2.0 * w);

    double num = c.b0 * c.b0 + c.b1 * c.b1 + c.b2 * c.b2
               + 2.0 * (c.b0 * c.b1 + c.b1 * c.b2) * cosW
               + 2.0 * c.b0 * c.b2 * cos2W;

    double den = 1.0 + c.a1 * c.a1 + c.a2 * c.a2
               + 2.0 * (c.a1 + c.a1 * c.a2) * cosW
               + 2.0 * c.a2 * cos2W;

    if (den <= 0.0) den = 1e-18;
    if (num < 0.0)  num = 1e-18;

    return static_cast<float>(10.0 * std::log10(num / den));
}

float ParametricEQDialog::compositeMagnitudeDb(float freq) const
{
    float total = 0.0f;
    for (int b = 0; b < NumBands; ++b) {
        total += bandMagnitudeDb(b, freq);
    }
    return total;
}

// ── Push parameters to the ParametricEQ model ─────────────────────────────

void ParametricEQDialog::pushBandToEQ(int band)
{
    if (!m_eq || band < 0 || band >= NumBands) return;

    int base = band * ParametricEQ::ParamsPerBand;
    m_eq->setParameter(base + 0, m_bands[band].freq);
    m_eq->setParameter(base + 1, m_bands[band].q);

    // If the band is disabled, push 0 dB gain to effectively bypass it
    float gain = m_bands[band].enabled ? m_bands[band].gainDb : 0.0f;
    m_eq->setParameter(base + 2, gain);
    m_eq->setParameter(base + 3, static_cast<float>(m_bands[band].type));
}

void ParametricEQDialog::syncControlsFromEQ()
{
    for (int b = 0; b < NumBands; ++b) {
        auto& w = m_widgets[b];
        if (w.freqKnob) w.freqKnob->setValue(m_bands[b].freq);
        if (w.gainKnob) w.gainKnob->setValue(m_bands[b].gainDb);
        if (w.qKnob)    w.qKnob->setValue(m_bands[b].q);
        if (w.typeCombo) w.typeCombo->setCurrentIndex(m_bands[b].type);
        if (w.enableCB)  w.enableCB->setChecked(m_bands[b].enabled);
    }
}

// ── Hit-Testing ───────────────────────────────────────────────────────────

int ParametricEQDialog::hitTestBand(int x, int y) const
{
    for (int b = 0; b < NumBands; ++b) {
        if (!m_bands[b].enabled) continue;

        int bx = freqToX(m_bands[b].freq);
        float mag = compositeMagnitudeDb(m_bands[b].freq);
        // Show point at the individual band's contribution to the composite curve
        // For better UX, we place the point at the band's own center frequency
        // on the composite curve
        int by = dbToY(mag);

        int dx = x - bx;
        int dy = y - by;
        if (dx * dx + dy * dy <= kPointHitRadius * kPointHitRadius) {
            return b;
        }
    }
    return -1;
}

// ── Paint ─────────────────────────────────────────────────────────────────

void ParametricEQDialog::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), QColor(30, 30, 36));

    QRect cr = curveRect();

    // Curve area background
    p.fillRect(cr, QColor(22, 22, 28));
    p.setPen(QPen(QColor(50, 50, 56), 1));
    p.drawRect(cr);

    // ── Grid: vertical lines at standard frequencies ──────────────────────

    static const float gridFreqs[] = {
        20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000
    };

    QFont gridFont = font();
    gridFont.setPointSize(7);
    p.setFont(gridFont);
    QFontMetrics fm(gridFont);

    for (float f : gridFreqs) {
        int x = freqToX(f);
        p.setPen(QPen(QColor(50, 50, 56), 1, Qt::DotLine));
        p.drawLine(x, cr.top(), x, cr.bottom());

        // Frequency label below
        p.setPen(QColor(120, 120, 130));
        QString label;
        if (f >= 1000)
            label = QString::number(static_cast<int>(f / 1000)) + QStringLiteral("k");
        else
            label = QString::number(static_cast<int>(f));
        int tw = fm.horizontalAdvance(label);
        p.drawText(x - tw / 2, cr.bottom() + 12, label);
    }

    // ── Grid: horizontal lines at dB values ───────────────────────────────

    static const float gridDbs[] = { -24, -18, -12, -6, 0, 6, 12, 18, 24 };

    for (float db : gridDbs) {
        int y = dbToY(db);
        bool isZero = (db == 0.0f);
        p.setPen(QPen(isZero ? QColor(80, 80, 90) : QColor(50, 50, 56),
                       isZero ? 2 : 1,
                       isZero ? Qt::SolidLine : Qt::DotLine));
        p.drawLine(cr.left(), y, cr.right(), y);

        // dB label on the left
        p.setPen(QColor(120, 120, 130));
        QString label = (db > 0 ? QStringLiteral("+") : QString())
                      + QString::number(static_cast<int>(db));
        int tw = fm.horizontalAdvance(label);
        p.drawText(cr.left() - tw - 6, y + 4, label);
    }

    // ── Individual band curves (semi-transparent) ─────────────────────────

    for (int b = 0; b < NumBands; ++b) {
        if (!m_bands[b].enabled) continue;

        QPainterPath bandPath;
        bool first = true;

        for (int px = 0; px <= kCurveWidth; ++px) {
            float freq = xToFreq(cr.left() + px);
            float db = bandMagnitudeDb(b, freq);
            db = std::clamp(db, kMinDb - 6.0f, kMaxDb + 6.0f);
            int y = dbToY(db);

            if (first) {
                bandPath.moveTo(cr.left() + px, y);
                first = false;
            } else {
                bandPath.lineTo(cr.left() + px, y);
            }
        }

        QColor bandColor = kBandColors[b];
        bandColor.setAlpha(b == m_selectedBand ? 120 : 50);
        p.setPen(QPen(bandColor, 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(bandPath);
    }

    // ── Composite frequency response curve ────────────────────────────────

    QPainterPath curvePath;
    QPainterPath fillPathAbove; // fill above 0dB
    QPainterPath fillPathBelow; // fill below 0dB

    int zeroY = dbToY(0.0f);

    bool firstCurve = true;
    for (int px = 0; px <= kCurveWidth; ++px) {
        float freq = xToFreq(cr.left() + px);
        float db = compositeMagnitudeDb(freq);
        db = std::clamp(db, kMinDb - 6.0f, kMaxDb + 6.0f);
        int y = dbToY(db);

        if (firstCurve) {
            curvePath.moveTo(cr.left() + px, y);
            firstCurve = false;
        } else {
            curvePath.lineTo(cr.left() + px, y);
        }
    }

    // Semi-transparent fill between curve and 0dB line
    {
        QPainterPath fillPath = curvePath;
        // Close back along the 0dB line
        fillPath.lineTo(cr.right(), zeroY);
        fillPath.lineTo(cr.left(), zeroY);
        fillPath.closeSubpath();

        // Clip to curve area
        p.save();
        p.setClipRect(cr);

        // Fill above 0dB (boost) in warm color
        QColor boostFill(100, 180, 255, 35);
        p.setBrush(boostFill);
        p.setPen(Qt::NoPen);
        p.drawPath(fillPath);

        p.restore();
    }

    // Draw the composite curve line
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(100, 180, 255), 2));
    p.save();
    p.setClipRect(cr);
    p.drawPath(curvePath);
    p.restore();

    // ── Band control points ───────────────────────────────────────────────

    for (int b = 0; b < NumBands; ++b) {
        if (!m_bands[b].enabled) continue;

        int bx = freqToX(m_bands[b].freq);
        float mag = compositeMagnitudeDb(m_bands[b].freq);
        int by = dbToY(mag);

        // Clamp to curve area
        by = std::clamp(by, cr.top(), cr.bottom());

        QColor color = kBandColors[b];
        bool isSelected = (b == m_selectedBand);

        // Outer ring
        p.setPen(QPen(isSelected ? color.lighter(140) : color, isSelected ? 2.5 : 1.5));
        p.setBrush(isSelected ? color : color.darker(160));
        p.drawEllipse(QPoint(bx, by), kPointRadius, kPointRadius);

        // Band number inside the point
        p.setPen(Qt::white);
        QFont ptFont = font();
        ptFont.setPointSize(7);
        ptFont.setBold(true);
        p.setFont(ptFont);
        QString numStr = QString::number(b + 1);
        int tw = fm.horizontalAdvance(numStr);
        p.drawText(bx - tw / 2, by + 3, numStr);
    }
}

// ── Mouse Interaction ─────────────────────────────────────────────────────

void ParametricEQDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QDialog::mousePressEvent(event);
        return;
    }

    QRect cr = curveRect();
    if (!cr.contains(event->pos())) {
        QDialog::mousePressEvent(event);
        return;
    }

    int band = hitTestBand(event->pos().x(), event->pos().y());
    if (band >= 0) {
        m_selectedBand = band;
        m_draggingBand = band;
        m_dragging = true;
    } else {
        m_selectedBand = -1;
    }
    update();
}

void ParametricEQDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging || m_draggingBand < 0) {
        QDialog::mouseMoveEvent(event);
        return;
    }

    int b = m_draggingBand;

    // Horizontal drag -> frequency
    float newFreq = xToFreq(event->pos().x());
    newFreq = std::clamp(newFreq, kMinFreq, kMaxFreq);
    m_bands[b].freq = newFreq;

    // Vertical drag -> gain
    float newGain = yToDb(event->pos().y());
    newGain = std::clamp(newGain, kMinDb, kMaxDb);
    m_bands[b].gainDb = newGain;

    // Update the knobs
    if (m_widgets[b].freqKnob) {
        m_widgets[b].freqKnob->blockSignals(true);
        m_widgets[b].freqKnob->setValue(newFreq);
        m_widgets[b].freqKnob->blockSignals(false);
    }
    if (m_widgets[b].gainKnob) {
        m_widgets[b].gainKnob->blockSignals(true);
        m_widgets[b].gainKnob->setValue(newGain);
        m_widgets[b].gainKnob->blockSignals(false);
    }

    pushBandToEQ(b);
    update();
}

void ParametricEQDialog::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    m_draggingBand = -1;
    QDialog::mouseReleaseEvent(event);
}

void ParametricEQDialog::mouseDoubleClickEvent(QMouseEvent* event)
{
    QRect cr = curveRect();
    if (!cr.contains(event->pos())) {
        QDialog::mouseDoubleClickEvent(event);
        return;
    }

    int band = hitTestBand(event->pos().x(), event->pos().y());
    if (band >= 0) {
        // Toggle enable/disable
        m_bands[band].enabled = !m_bands[band].enabled;
        if (m_widgets[band].enableCB) {
            m_widgets[band].enableCB->blockSignals(true);
            m_widgets[band].enableCB->setChecked(m_bands[band].enabled);
            m_widgets[band].enableCB->blockSignals(false);
        }
        pushBandToEQ(band);
        update();
    }
}

void ParametricEQDialog::wheelEvent(QWheelEvent* event)
{
    QRect cr = curveRect();
    QPoint pos = event->position().toPoint();
    if (!cr.contains(pos)) {
        QDialog::wheelEvent(event);
        return;
    }

    // Find nearest band to mouse
    int band = hitTestBand(pos.x(), pos.y());
    if (band < 0 && m_selectedBand >= 0) {
        band = m_selectedBand; // Use selected band if no direct hit
    }
    if (band < 0) {
        QDialog::wheelEvent(event);
        return;
    }

    // Adjust Q
    float delta = (event->angleDelta().y() > 0) ? 0.1f : -0.1f;
    float newQ = m_bands[band].q + delta;
    newQ = std::clamp(newQ, 0.1f, 20.0f);
    m_bands[band].q = newQ;

    if (m_widgets[band].qKnob) {
        m_widgets[band].qKnob->blockSignals(true);
        m_widgets[band].qKnob->setValue(newQ);
        m_widgets[band].qKnob->blockSignals(false);
    }

    pushBandToEQ(band);
    update();
    event->accept();
}

} // namespace dawcast::widgets
