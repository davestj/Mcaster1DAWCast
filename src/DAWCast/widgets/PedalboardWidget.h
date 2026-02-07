// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QList>
#include <QString>

class QHBoxLayout;
class QScrollArea;
class QPushButton;

namespace dawcast {
class IEffectUnit;
class DspChain;
}

namespace dawcast::widgets {

class EmbossedKnob;

// ── Single Pedal (stompbox) ────────────────────────────────────────────────

/// A skeuomorphic stompbox widget: colored faceplate, effect name, LED
/// indicator, bypass footswitch, and 3-4 parameter knobs.
class PedalWidget : public QWidget {
    Q_OBJECT

public:
    explicit PedalWidget(const QString& effectName,
                         const QColor& faceplateColor,
                         QWidget* parent = nullptr);
    ~PedalWidget() override;

    void setEffectUnit(IEffectUnit* effect);
    IEffectUnit* effectUnit() const;

    bool isBypassed() const;
    void setBypassed(bool bypassed);

    QString effectName() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void bypassToggled(bool bypassed);
    void parameterChanged(int id, float value);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void buildUI();
    void syncKnobsFromEffect();
    void syncKnobsToEffect();

    QString     m_effectName;
    QColor      m_faceplateColor;
    IEffectUnit* m_effect = nullptr;
    bool        m_bypassed = false;

    QList<EmbossedKnob*> m_knobs;
    QPushButton*         m_footswitch = nullptr;
};

// ── Pedalboard (horizontal chain) ──────────────────────────────────────────

/// Horizontal scrollable chain of PedalWidget stompboxes.
/// Represents the signal chain for Guitar FX mode.
class PedalboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit PedalboardWidget(QWidget* parent = nullptr);
    ~PedalboardWidget() override;

    /// Add a pedal to the end of the chain.
    PedalWidget* addPedal(const QString& effectName, const QColor& color);

    /// Remove a pedal by index.
    void removePedal(int index);

    /// Number of pedals in the chain.
    int pedalCount() const;

    /// Access a pedal by index.
    PedalWidget* pedal(int index) const;

    /// Wire to a DspChain — future integration point.
    void setDspChain(dawcast::DspChain* chain);

signals:
    void chainChanged();

private:
    void buildDefaultChain();
    void addPedalButton();

    QScrollArea*        m_scrollArea   = nullptr;
    QWidget*            m_chainWidget  = nullptr;
    QHBoxLayout*        m_chainLayout  = nullptr;
    QPushButton*        m_addButton    = nullptr;
    QList<PedalWidget*> m_pedals;

    dawcast::DspChain*  m_dspChain     = nullptr;
};

} // namespace dawcast::widgets
