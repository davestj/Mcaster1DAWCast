/*
 * Mcaster1DAWCast — MC1 Podcast Plugin Family
 * fx_ui/PodcastPluginDialog.h — generic editor for the 9 podcast plugins
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * One generic Q_OBJECT dialog used by all 9 MC1 podcast plugins. Each
 * one introspects its underlying mc1dsp::DspEffect via paramCount() /
 * paramName() / paramDisplayValue() so the same dialog code renders
 * the right knobs for the right plugin.
 *
 * Each plugin gets its own thin Q_OBJECT subclass below the base so
 * MOC can generate distinct meta-objects (lets us register them in
 * the dialog factory by id).
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "patchbay/dsp/dsp_effect.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QString>

class PodcastPluginDialog : public QDialog {
    Q_OBJECT

public:
    explicit PodcastPluginDialog(mc1dsp::DspEffect* fx,
                                 const QString& title,
                                 const QString& subtitle,
                                 QWidget* parent = nullptr)
        : QDialog(parent)
        , fx_(fx)
        , title_(title)
        , subtitle_(subtitle)
    {
        setWindowTitle(title);
        if (!fx_) return;

        const int n = fx_->paramCount();
        // Sizing: title bar + one row of knobs (78 px each + spacing) + bottom
        const int width = qMax(560, 80 + n * 92);
        setFixedSize(width, 280);

        applyTheme();
        buildUi();

        // Periodic poll to keep displayed values in sync if other code
        // (e.g. the registry default) changes them.
        m_poll = new QTimer(this);
        m_poll->setInterval(150);
        connect(m_poll, &QTimer::timeout, this, &PodcastPluginDialog::pollValues);
        m_poll->start();
    }

private:
    void applyTheme()
    {
        setStyleSheet(
            "QDialog { background: #0c1422; color: #d6e4f0; }"
            "QGroupBox {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "      stop:0 #102036, stop:1 #0a1626);"
            "  border: 1px solid #20406a; border-left: 3px solid #ffb020;"
            "  border-radius: 6px; margin-top: 14px;"
            "  padding: 10px 6px 6px 6px; font-size: 11px; color: #6088b0;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin; subcontrol-position: top left;"
            "  padding: 2px 8px; color: #ffb020; font-weight: bold;"
            "  letter-spacing: 1px;"
            "}"
            "QLabel { color: #d6e4f0; }"
            "QPushButton {"
            "  background: #102036; color: #d6e4f0;"
            "  border: 1px solid #20406a; border-radius: 3px;"
            "  padding: 6px 16px; min-width: 70px;"
            "}"
            "QPushButton:hover { background: #1a2c4a; }"
            "QPushButton:pressed { background: #ffb020; color: #0c1422; }"
        );
    }

    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(8);

        // Header
        auto* headerRow = new QHBoxLayout;
        auto* titleLabel = new QLabel(title_.toUpper());
        titleLabel->setStyleSheet(
            "font-size: 16px; font-weight: bold; color: #ffb020; letter-spacing: 4px;");
        headerRow->addWidget(titleLabel);
        headerRow->addStretch();
        if (!subtitle_.isEmpty()) {
            auto* sub = new QLabel(subtitle_);
            sub->setStyleSheet("color: #6088b0; font-style: italic;");
            headerRow->addWidget(sub);
        }
        root->addLayout(headerRow);

        // Knob row
        auto* knobGroup = new QGroupBox("CONTROLS");
        auto* knobLayout = new QHBoxLayout(knobGroup);
        knobLayout->setSpacing(2);

        const int n = fx_->paramCount();
        for (int i = 0; i < n; ++i) {
            auto* k = new RackKnob;
            k->setTitle(QString::fromLatin1(fx_->paramName(i)).toUpper());
            k->setFixedSize(78, 110);
            k->setValue(fx_->paramValue(i));
            int paramIdx = i;
            connect(k, &RackKnob::valueChanged, this, [this, paramIdx](float v) {
                if (fx_) fx_->setParamValue(paramIdx, v);
            });
            m_knobs.append(k);
            knobLayout->addWidget(k);
        }
        knobLayout->addStretch();
        root->addWidget(knobGroup, 1);

        // Bottom row: bypass + close
        auto* bottomRow = new QHBoxLayout;
        auto* bypassBtn = new QPushButton("BYPASS");
        bypassBtn->setCheckable(true);
        connect(bypassBtn, &QPushButton::toggled, this, [this](bool on) {
            if (fx_) fx_->setBypassed(on);
        });
        bottomRow->addWidget(bypassBtn);
        bottomRow->addStretch();
        auto* resetBtn = new QPushButton("RESET");
        connect(resetBtn, &QPushButton::clicked, this, &PodcastPluginDialog::resetParams);
        bottomRow->addWidget(resetBtn);
        auto* closeBtn = new QPushButton("CLOSE");
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
        bottomRow->addWidget(closeBtn);
        root->addLayout(bottomRow);
    }

    void pollValues()
    {
        if (!fx_) return;
        for (int i = 0; i < m_knobs.size() && i < fx_->paramCount(); ++i) {
            float v = fx_->paramValue(i);
            // Only update tooltip with display value — don't fight the user
            m_knobs[i]->setToolTip(
                QString("%1: %2")
                    .arg(QString::fromLatin1(fx_->paramName(i)),
                         QString::fromStdString(fx_->paramDisplayValue(i))));
        }
    }

    void resetParams()
    {
        // Each plugin's default values are set in its constructor; the
        // simplest "reset" is to set every param to its center (0.5).
        // Plugins will recompute on each setParamValue.
        if (!fx_) return;
        for (int i = 0; i < fx_->paramCount(); ++i) {
            fx_->setParamValue(i, 0.5f);
            if (i < m_knobs.size()) m_knobs[i]->setValue(0.5f);
        }
    }

protected:
    mc1dsp::DspEffect* fx_ = nullptr;
    QString title_;
    QString subtitle_;
    QList<RackKnob*> m_knobs;
    QTimer* m_poll = nullptr;
};

/* ── Per-plugin Q_OBJECT subclasses so MOC generates distinct types ── */

class VoiceLiftDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    VoiceLiftDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Voice Lift Pro",
                              "Clean +25 dB inline gain for low-output dynamic mics",
                              parent) {}
};

class PlosiveKillerDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    PlosiveKillerDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Plosive Killer",
                              "Transient-detection plosive suppressor",
                              parent) {}
};

class MouthClickDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    MouthClickDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Mouth Click Remover",
                              "Spectral mouth click + lip smack remover",
                              parent) {}
};

class BleedSuppressorDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    BleedSuppressorDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Multi-Host Bleed Suppressor",
                              "Sidechain bleed gate for roundtable podcasts",
                              parent) {}
};

class PhoneLineDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    PhoneLineDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Phone Line Sim",
                              "Telephony / VoIP / cellphone simulator",
                              parent) {}
};

class RemoteRestorerDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    RemoteRestorerDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Remote Guest Restorer",
                              "Reverb suppression + de-noise for remote guests",
                              parent) {}
};

class LoudnessMatchDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    LoudnessMatchDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Loudness Match",
                              "EBU R128 short-term loudness aligner",
                              parent) {}
};

class StingerBedDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    StingerBedDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Stinger Bed",
                              "Auto-ducking music bed for podcasts",
                              parent) {}
};

class VodcastLipsyncDialog : public PodcastPluginDialog {
    Q_OBJECT
public:
    VodcastLipsyncDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : PodcastPluginDialog(fx, "MC1 Vodcast Lipsync",
                              "Audio-to-video drift corrector",
                              parent) {}
};
