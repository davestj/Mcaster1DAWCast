/*
 * Mcaster1DAWCast — MC1 Studios Family
 * fx_ui/MusicStudioDialogs.h — flagship UIs for the 4 music studios
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Custom QPainter-rendered "live room" hero widget shared by all 4
 * music studios (Tidemark A, Tidemark B, Tidemark Vault, Granite A).
 * Each studio subclasses the dialog with its own profile, room
 * dimensions, color palette, and source-position labels.
 */

#pragma once

#include "fx_ui/RackKnob.h"
#include "patchbay/dsp/dsp_effect.h"
#include "patchbay/dsp/preset_manager.h"
#include "patchbay/dsp/fx_mc1_tidemark_a.h"
#include "patchbay/dsp/fx_mc1_tidemark_b.h"
#include "patchbay/dsp/fx_mc1_tidemark_vault.h"
#include "patchbay/dsp/fx_mc1_granite_a.h"

#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPaintEvent>
#include <QTimer>
#include <QStringList>

/* ─────────────────────────────────────────────────────────────────
 *  StudioRoomView — shared QPainter widget that draws an isometric
 *  view of a music studio live room with the source position, mic,
 *  console, and chamber visible. Each studio configures it via
 *  setProfile().
 * ───────────────────────────────────────────────────────────────── */
class StudioRoomView : public QWidget {
    Q_OBJECT

public:
    struct Profile {
        QString    studioName;
        QString    tagline;
        QColor     wallColor;
        QColor     accentColor;
        bool       hasChamber = true;
        QStringList sourceNames;
        QStringList micNames;
        int        roomWidthScale = 320;   // base width of live room
        int        roomHeightScale = 160;
    };

    explicit StudioRoomView(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(720, 200);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setProfile(const Profile& p) { m_profile = p; update(); }
    void setSourcePosition(int p)     { m_pos = p;     update(); }
    void setMicSelection(int m)       { m_mic = m;     update(); }
    void setChamberSend(float v)      { m_chamberMix = v; update(); }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRect r = rect();
        const int W = r.width();
        const int H = r.height();

        // Background — rich studio control room ambience
        QLinearGradient bg(0, 0, 0, H);
        bg.setColorAt(0.0, m_profile.wallColor.darker(180));
        bg.setColorAt(0.5, m_profile.wallColor.darker(220));
        bg.setColorAt(1.0, QColor(0x06, 0x0c, 0x18));
        p.fillRect(r, bg);

        // Top spotlight (warm)
        QRadialGradient spot(W * 0.5, -20, W * 0.7);
        spot.setColorAt(0.0, m_profile.accentColor.lighter(120));
        spot.setColorAt(1.0, QColor(0, 0, 0, 0));
        QColor sc = m_profile.accentColor;
        sc.setAlpha(28);
        p.setBrush(sc);
        p.setPen(Qt::NoPen);
        p.drawRect(r);

        // ── Live room (left half) ─────────────────────────────────
        const int roomX = 30;
        const int roomY = 30;
        const int roomW = m_profile.roomWidthScale;
        const int roomH = std::min(m_profile.roomHeightScale, H - 70);

        // Trapezoid for isometric depth
        QPolygonF room;
        room << QPointF(roomX,            roomY)
             << QPointF(roomX + roomW,    roomY + 14)
             << QPointF(roomX + roomW,    roomY + roomH)
             << QPointF(roomX,            roomY + roomH - 14);

        QLinearGradient rg(roomX, roomY, roomX, roomY + roomH);
        rg.setColorAt(0.0, m_profile.wallColor.lighter(115));
        rg.setColorAt(0.5, m_profile.wallColor);
        rg.setColorAt(1.0, m_profile.wallColor.darker(140));
        p.setBrush(rg);
        p.setPen(QPen(m_profile.accentColor.darker(200), 2));
        p.drawPolygon(room);

        // Wall texture (subtle dot grid)
        p.setPen(QPen(m_profile.accentColor.darker(180).lighter(130), 1));
        for (int gy = roomY + 18; gy < roomY + roomH - 24; gy += 14) {
            for (int gx = roomX + 16; gx < roomX + roomW - 12; gx += 14) {
                p.drawPoint(gx, gy);
            }
        }

        // Studio name label inside the room
        p.setPen(m_profile.accentColor);
        QFont nameFont = font();
        nameFont.setPointSize(8);
        nameFont.setBold(true);
        nameFont.setLetterSpacing(QFont::PercentageSpacing, 110);
        p.setFont(nameFont);
        p.drawText(roomX + 8, roomY + 16, m_profile.studioName.toUpper());

        // ── Source position markers ───────────────────────────────
        // Distribute up to 5 source positions across the room
        const int posCount = m_profile.sourceNames.size();
        for (int i = 0; i < posCount; ++i) {
            float fx = static_cast<float>(i + 1) / static_cast<float>(posCount + 1);
            int sx = roomX + static_cast<int>(fx * roomW);
            int sy = roomY + roomH / 2;

            bool active = (i == m_pos);
            QColor markerCol = active ? m_profile.accentColor
                                      : m_profile.accentColor.darker(180);

            // Draw marker as a small filled square (instrument position)
            p.setBrush(markerCol);
            p.setPen(QPen(markerCol.lighter(140), active ? 2 : 1));
            int sz = active ? 12 : 8;
            p.drawEllipse(QPointF(sx, sy), sz, sz);

            if (active) {
                // Draw the active position label below
                p.setPen(m_profile.accentColor);
                QFont posFont("Menlo");
                posFont.setPointSize(7);
                posFont.setBold(true);
                p.setFont(posFont);
                QFontMetrics fm(posFont);
                QString label = m_profile.sourceNames[i].toUpper();
                int textW = fm.horizontalAdvance(label);
                p.drawText(sx - textW / 2, sy + 24, label);

                // Draw cardioid pickup pattern around the marker
                QPainterPath card;
                card.moveTo(sx, sy - 16);
                card.cubicTo(sx + 18, sy - 10, sx + 18, sy + 10, sx, sy + 16);
                card.cubicTo(sx - 18, sy + 10, sx - 18, sy - 10, sx, sy - 16);
                p.setPen(QPen(m_profile.accentColor, 1, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawPath(card);
            }
        }

        // ── Mic name plate (bottom of the room) ──────────────────
        if (m_mic >= 0 && m_mic < m_profile.micNames.size()) {
            p.setPen(m_profile.accentColor);
            QFont micFont("Menlo");
            micFont.setPointSize(9);
            micFont.setBold(true);
            p.setFont(micFont);
            QFontMetrics fm(micFont);
            QString micName = m_profile.micNames[m_mic];
            int textW = fm.horizontalAdvance(micName);
            int micX = roomX + roomW / 2 - textW / 2;
            int micY = roomY + roomH - 8;

            // LCD plate
            QRect plate(micX - 8, micY - 12, textW + 16, 16);
            p.setBrush(QColor(0x06, 0x0a, 0x14));
            p.setPen(QPen(m_profile.accentColor.darker(140), 1));
            p.drawRoundedRect(plate, 2, 2);
            p.setPen(m_profile.accentColor);
            p.drawText(plate, Qt::AlignCenter, micName);
        }

        // ── Control room (right half) ─────────────────────────────
        const int conX = roomX + roomW + 24;
        const int conY = roomY;
        const int conW = W - conX - 30;
        const int conH = roomH;

        QLinearGradient cg(conX, conY, conX, conY + conH);
        cg.setColorAt(0.0, QColor(0x14, 0x20, 0x36));
        cg.setColorAt(1.0, QColor(0x06, 0x0a, 0x14));
        p.setBrush(cg);
        p.setPen(QPen(QColor(0x20, 0x40, 0x6a), 2));
        p.drawRoundedRect(QRect(conX, conY, conW, conH), 6, 6);

        // Vintage console (slanted operator surface)
        const int conTop = conY + 14;
        const int conSurfH = conH / 2 - 8;
        QPolygonF console;
        console << QPointF(conX + 10,        conTop)
                << QPointF(conX + conW - 10, conTop)
                << QPointF(conX + conW - 18, conTop + conSurfH)
                << QPointF(conX + 18,        conTop + conSurfH);
        QLinearGradient consoleGrad(conX, conTop, conX, conTop + conSurfH);
        consoleGrad.setColorAt(0.0, QColor(0x40, 0x30, 0x20));  // wood-tone vintage console
        consoleGrad.setColorAt(1.0, QColor(0x20, 0x18, 0x10));
        p.setBrush(consoleGrad);
        p.setPen(QPen(QColor(0x60, 0x48, 0x30), 1));
        p.drawPolygon(console);

        // Channel strips on the console
        for (int i = 0; i < 16; ++i) {
            int cx = conX + 22 + i * (conW - 44) / 15;
            int cy = conTop + conSurfH / 2;
            p.setBrush(m_profile.accentColor);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(cx, cy - 6), 1.5, 1.5);
            p.drawEllipse(QPointF(cx, cy), 1.5, 1.5);
            p.setPen(QPen(QColor(0xa0, 0x80, 0x60), 1));
            p.drawLine(cx, cy + 6, cx, cy + 16);
        }

        // Outboard rack (bottom half)
        const int rackTop = conTop + conSurfH + 6;
        const int rackH = conY + conH - rackTop - 10;
        QRect rack(conX + 10, rackTop, conW - 20, rackH);
        QLinearGradient rackGrad(0, rackTop, 0, rackTop + rackH);
        rackGrad.setColorAt(0.0, QColor(0x12, 0x18, 0x24));
        rackGrad.setColorAt(1.0, QColor(0x06, 0x0a, 0x14));
        p.setBrush(rackGrad);
        p.setPen(QPen(QColor(0x30, 0x38, 0x48), 1));
        p.drawRect(rack);

        // 3 rack units
        const int unitH = (rackH - 12) / 3;
        for (int u = 0; u < 3; ++u) {
            int uy = rackTop + 4 + u * (unitH + 2);
            QRect unit(rack.x() + 4, uy, rack.width() - 8, unitH - 2);
            QLinearGradient ug(0, uy, 0, uy + unitH);
            ug.setColorAt(0.0, QColor(0x20, 0x28, 0x38));
            ug.setColorAt(1.0, QColor(0x10, 0x16, 0x22));
            p.setBrush(ug);
            p.setPen(QPen(QColor(0x40, 0x50, 0x68), 1));
            p.drawRoundedRect(unit, 2, 2);
            p.setBrush(m_profile.accentColor);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(unit.left() + 8, unit.center().y()), 2.5, 2.5);
        }

        // ── Chamber send indicator (right edge of console area) ──
        if (m_profile.hasChamber && m_chamberMix > 0.05f) {
            int chX = conX + conW - 50;
            int chY = conY + 6;
            QRect chamberBadge(chX, chY, 44, 14);
            p.setBrush(m_profile.accentColor);
            p.setPen(QPen(QColor(0xff, 0xff, 0xff), 1));
            p.drawRoundedRect(chamberBadge, 3, 3);
            p.setPen(QColor(0xff, 0xff, 0xff));
            QFont cf("Menlo");
            cf.setPointSize(7);
            cf.setBold(true);
            p.setFont(cf);
            p.drawText(chamberBadge, Qt::AlignCenter, "CHAMBER");
        }

        // Tagline
        p.setPen(QColor(0x60, 0x88, 0xb0));
        QFont tlf = font();
        tlf.setPointSize(8);
        tlf.setItalic(true);
        p.setFont(tlf);
        p.drawText(W - 220, H - 8, m_profile.tagline);
    }

private:
    Profile m_profile;
    int     m_pos = 0;
    int     m_mic = 0;
    float   m_chamberMix = 0.0f;
};

/* ─────────────────────────────────────────────────────────────────
 *  MusicStudioDialog — shared base for music studio editor dialogs.
 *  Renders the room view + 3 knob banks + bottom row.
 * ───────────────────────────────────────────────────────────────── */
class MusicStudioDialog : public QDialog {
    Q_OBJECT

public:
    explicit MusicStudioDialog(mc1dsp::DspEffect* fx,
                               const StudioRoomView::Profile& profile,
                               int paramCount,
                               const QStringList& bank1,
                               const QStringList& bank2,
                               const QStringList& bank3,
                               QWidget* parent = nullptr)
        : QDialog(parent)
        , m_fx(fx)
        , m_paramCount(paramCount)
    {
        setWindowTitle(profile.studioName);
        resize(1000, 580);
        setMinimumSize(700, 420);
        applyTheme();
        buildUi(profile, bank1, bank2, bank3);
        loadFromEffect();
        refreshPresetList();

        m_poll = new QTimer(this);
        m_poll->setInterval(120);
        connect(m_poll, &QTimer::timeout, this, &MusicStudioDialog::pollDisplay);
        m_poll->start();
    }

private:
    void applyTheme()
    {
        setStyleSheet(
            "QDialog { background: #0a1220; color: #d6e4f0; }"
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
            "QComboBox {"
            "  background: #102036; color: #d6e4f0;"
            "  border: 1px solid #20406a; border-radius: 3px;"
            "  padding: 4px 8px; min-width: 160px;"
            "}"
            "QComboBox QAbstractItemView { background: #0a1220; color: #d6e4f0; }"
            "QPushButton {"
            "  background: #102036; color: #d6e4f0;"
            "  border: 1px solid #20406a; border-radius: 3px;"
            "  padding: 6px 16px; min-width: 60px;"
            "}"
            "QPushButton:hover { background: #1a2c4a; }"
            "QPushButton:pressed { background: #ffb020; color: #0c1422; }"
        );
    }

    void buildUi(const StudioRoomView::Profile& profile,
                 const QStringList& bank1,
                 const QStringList& bank2,
                 const QStringList& bank3)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(14, 10, 14, 10);
        root->setSpacing(8);

        // Header row: title + preset selector
        auto* header = new QHBoxLayout;
        auto* title = new QLabel(profile.studioName.toUpper());
        title->setStyleSheet(
            "font-size: 17px; font-weight: bold; color: #ffb020; letter-spacing: 5px;");
        header->addWidget(title);
        header->addStretch();

        // Preset combo
        auto* presetLabel = new QLabel("PRESET:");
        presetLabel->setStyleSheet("color: #6088b0; font-size: 10px; font-weight: bold;");
        header->addWidget(presetLabel);
        m_presetCombo = new QComboBox;
        m_presetCombo->setToolTip("Load a preset");
        connect(m_presetCombo, QOverload<int>::of(&QComboBox::activated),
                this, &MusicStudioDialog::onPresetSelected);
        header->addWidget(m_presetCombo);

        auto* saveBtn = new QPushButton("SAVE");
        saveBtn->setFixedWidth(50);
        saveBtn->setToolTip("Save current settings as a preset");
        connect(saveBtn, &QPushButton::clicked, this, &MusicStudioDialog::onSavePreset);
        header->addWidget(saveBtn);

        auto* deleteBtn = new QPushButton("DEL");
        deleteBtn->setFixedWidth(40);
        deleteBtn->setToolTip("Delete selected custom preset");
        connect(deleteBtn, &QPushButton::clicked, this, &MusicStudioDialog::onDeletePreset);
        header->addWidget(deleteBtn);

        root->addLayout(header);

        // Tagline
        auto* tagline = new QLabel(profile.tagline);
        tagline->setStyleSheet("color: #6088b0; font-style: italic; font-size: 11px;");
        root->addWidget(tagline);

        // Hero room view
        m_roomView = new StudioRoomView();
        m_roomView->setProfile(profile);
        root->addWidget(m_roomView, 1);

        // Knob banks
        auto* banks = new QHBoxLayout;
        banks->setSpacing(8);
        addKnobBank(banks, "POSITION  &  MIC", bank1);
        addKnobBank(banks, "CONSOLE  &  CHAIN", bank2, true);
        addKnobBank(banks, "DELIVERY", bank3);
        root->addLayout(banks);

        // Bottom row: bypass + apply + status + close
        auto* bottom = new QHBoxLayout;
        auto* bypass = new QPushButton("BYPASS");
        bypass->setCheckable(true);
        connect(bypass, &QPushButton::toggled, this, [this](bool on) {
            if (m_fx) m_fx->setBypassed(on);
        });
        bottom->addWidget(bypass);

        auto* resetBtn = new QPushButton("RESET");
        resetBtn->setToolTip("Reset all parameters to defaults");
        connect(resetBtn, &QPushButton::clicked, this, [this]() {
            if (!m_fx) return;
            m_fx->reset();
            loadFromEffect();
        });
        bottom->addWidget(resetBtn);

        bottom->addStretch();
        m_statusLabel = new QLabel("READY");
        m_statusLabel->setStyleSheet(
            "QLabel { color: #ffb020; font-family: 'Menlo'; font-size: 11px; "
            "background: #06101e; padding: 4px 12px; border: 1px solid #20406a; "
            "border-radius: 3px; }");
        bottom->addWidget(m_statusLabel);
        bottom->addStretch();

        auto* close = new QPushButton("CLOSE");
        connect(close, &QPushButton::clicked, this, &QDialog::close);
        bottom->addWidget(close);
        root->addLayout(bottom);
    }

    void addKnobBank(QHBoxLayout* parent, const QString& title,
                     const QStringList& specs, bool stretch = false)
    {
        auto* group = new QGroupBox(title);
        auto* layout = new QHBoxLayout(group);
        layout->setSpacing(2);
        for (const QString& spec : specs) {
            auto parts = spec.split(':');
            if (parts.size() != 2) continue;
            int paramIdx = parts[1].toInt();
            if (paramIdx < 0 || paramIdx >= m_paramCount) continue;
            auto* k = new RackKnob;
            k->setTitle(parts[0]);
            k->setFixedSize(78, 110);
            connect(k, &RackKnob::valueChanged, this, [this, paramIdx](float v) {
                if (m_fx) m_fx->setParamValue(paramIdx, v);
                updateRoomView();
            });
            m_knobs.append(KnobEntry{ paramIdx, k });
            layout->addWidget(k);
        }
        parent->addWidget(group, stretch ? 1 : 0);
    }

    // ── Preset management ────────────────────────────────────────
    void refreshPresetList()
    {
        if (!m_fx || !m_presetCombo) return;
        m_presetCombo->clear();
        m_presetCombo->addItem("-- Default --");
        m_presets = mc1dsp::PresetManager::listPresets(
            QString::fromLatin1(m_fx->id()));
        for (const auto& p : m_presets)
            m_presetCombo->addItem(
                (p.isFactory ? QString("[F] ") : QString()) + p.name);
    }

    void onPresetSelected(int index)
    {
        if (index <= 0 || !m_fx) return; // 0 = "-- Default --"
        int presetIdx = index - 1;
        if (presetIdx >= 0 && presetIdx < m_presets.size()) {
            mc1dsp::PresetManager::applyPreset(m_presets[presetIdx], m_fx);
            loadFromEffect();
            if (m_statusLabel)
                m_statusLabel->setText(
                    QString("Loaded: %1").arg(m_presets[presetIdx].name));
        }
    }

    void onSavePreset()
    {
        if (!m_fx) return;
        bool ok = false;
        QString name = QInputDialog::getText(
            this, "Save Preset", "Preset name:",
            QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        auto preset = mc1dsp::PresetManager::capturePreset(m_fx, name.trimmed());
        if (mc1dsp::PresetManager::savePreset(preset)) {
            refreshPresetList();
            if (m_statusLabel)
                m_statusLabel->setText(QString("Saved: %1").arg(name));
        }
    }

    void onDeletePreset()
    {
        if (!m_presetCombo) return;
        int idx = m_presetCombo->currentIndex() - 1;
        if (idx < 0 || idx >= m_presets.size()) return;
        if (m_presets[idx].isFactory) {
            QMessageBox::warning(this, "Cannot Delete",
                "Factory presets cannot be deleted.");
            return;
        }
        mc1dsp::PresetManager::deletePreset(m_presets[idx].filePath);
        refreshPresetList();
        if (m_statusLabel) m_statusLabel->setText("Preset deleted");
    }

    void loadFromEffect()
    {
        if (!m_fx) return;
        for (auto& k : m_knobs)
            k.knob->setValue(m_fx->paramValue(k.paramIdx));
        updateRoomView();
    }

    void updateRoomView()
    {
        if (!m_fx || !m_roomView) return;
        if (m_paramCount > 0)
            m_roomView->setSourcePosition(positionFromParam(m_fx->paramValue(0)));
        if (m_paramCount > 1)
            m_roomView->setMicSelection(micFromParam(m_fx->paramValue(1)));
        if (m_paramCount > 7)
            m_roomView->setChamberSend(m_fx->paramValue(7));
    }

    virtual int positionFromParam(float v) const
    {
        return std::max(0, std::min(4, static_cast<int>(v * 4.999f)));
    }

    virtual int micFromParam(float v) const
    {
        return std::max(0, std::min(3, static_cast<int>(v * 3.999f)));
    }

    void pollDisplay()
    {
        if (!m_fx) return;
        for (auto& k : m_knobs)
            k.knob->setToolTip(
                QString("%1: %2")
                    .arg(QString::fromLatin1(m_fx->paramName(k.paramIdx)),
                         QString::fromStdString(m_fx->paramDisplayValue(k.paramIdx))));
    }

protected:
    struct KnobEntry { int paramIdx; RackKnob* knob; };

    mc1dsp::DspEffect*       m_fx = nullptr;
    int                      m_paramCount = 0;
    StudioRoomView*          m_roomView = nullptr;
    QList<KnobEntry>         m_knobs;
    QTimer*                  m_poll = nullptr;
    QComboBox*               m_presetCombo = nullptr;
    QLabel*                  m_statusLabel = nullptr;
    QVector<mc1dsp::Preset>  m_presets;
};

/* ── Per-studio Q_OBJECT subclasses ─────────────────────────────── */

class TidemarkAStudioDialog : public MusicStudioDialog {
    Q_OBJECT
public:
    TidemarkAStudioDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : MusicStudioDialog(fx, makeProfile(), mc1dsp::FxTidemarkA::kParamCount,
            QStringList{"POSITION:0", "MIC:1", "PROXIMITY:2", "ROOM:6"},
            QStringList{"CONSOLE:3", "EQ:4", "COMP:5", "CHAMBER:7", "DECAY:8", "POLISH:9"},
            QStringList{"MIX:10", "OUTPUT:11"},
            parent) {}

private:
    static StudioRoomView::Profile makeProfile()
    {
        StudioRoomView::Profile p;
        p.studioName  = "MC1 TIDEMARK STUDIOS A";
        p.tagline     = "California coast — big live tracking room";
        p.wallColor   = QColor(0x18, 0x28, 0x42);
        p.accentColor = QColor(0xff, 0xb0, 0x20);
        p.hasChamber  = true;
        p.sourceNames = {"Drum Riser", "Vocal Booth", "Live Front", "Live Rear", "Iso Booth"};
        p.micNames    = {"U47", "C12", "SM57", "RIBBON 121"};
        p.roomWidthScale = 360;
        return p;
    }
};

class TidemarkBStudioDialog : public MusicStudioDialog {
    Q_OBJECT
public:
    TidemarkBStudioDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : MusicStudioDialog(fx, makeProfile(), mc1dsp::FxTidemarkB::kParamCount,
            QStringList{"POSITION:0", "MIC:1", "PROXIMITY:2", "ROOM:6"},
            QStringList{"CONSOLE:3", "EQ:4", "COMP:5", "CHAMBER:7", "DECAY:8", "POLISH:9"},
            QStringList{"MIX:10", "OUTPUT:11"},
            parent) {}

private:
    static StudioRoomView::Profile makeProfile()
    {
        StudioRoomView::Profile p;
        p.studioName  = "MC1 TIDEMARK STUDIOS B";
        p.tagline     = "Smaller mid-room — R&B / soul / singer-songwriter";
        p.wallColor   = QColor(0x14, 0x22, 0x38);
        p.accentColor = QColor(0xff, 0xb0, 0x20);
        p.hasChamber  = true;
        p.sourceNames = {"Vocal Close", "Vocal Back", "Instrument"};
        p.micNames    = {"U47", "C12", "SM57", "RIBBON 121"};
        p.roomWidthScale = 280;
        return p;
    }
};

class TidemarkVaultDialog : public MusicStudioDialog {
    Q_OBJECT
public:
    TidemarkVaultDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : MusicStudioDialog(fx, makeProfile(), mc1dsp::FxTidemarkVault::kParamCount,
            QStringList{"CHAMBER:0", "DECAY:1", "PRE-DELAY:2"},
            QStringList{"SIZE:3", "DAMPING:4", "LOW CUT:5"},
            QStringList{"MIX:6", "OUTPUT:7"},
            parent) {}

private:
    static StudioRoomView::Profile makeProfile()
    {
        StudioRoomView::Profile p;
        p.studioName  = "MC1 TIDEMARK VAULT CHAMBERS";
        p.tagline     = "Three underground brick reverb chambers";
        p.wallColor   = QColor(0x32, 0x18, 0x14);  // brick reddish-brown
        p.accentColor = QColor(0xff, 0xa0, 0x60);  // copper
        p.hasChamber  = false;  // it IS the chamber
        p.sourceNames = {"Chamber A", "Chamber B", "Chamber C"};
        p.micNames    = {""};
        p.roomWidthScale = 320;
        return p;
    }
};

class GraniteAStudioDialog : public MusicStudioDialog {
    Q_OBJECT
public:
    GraniteAStudioDialog(mc1dsp::DspEffect* fx, QWidget* parent = nullptr)
        : MusicStudioDialog(fx, makeProfile(), mc1dsp::FxGraniteA::kParamCount,
            QStringList{"POSITION:0", "MIC:1", "PROXIMITY:2"},
            QStringList{"CONSOLE:3", "EQ:4", "1176:5", "DOLBY A:6", "ROOM:7", "CHAMBER:8", "DECAY:9", "POLISH:10"},
            QStringList{"MIX:11", "OUTPUT:12"},
            parent) {}

private:
    static StudioRoomView::Profile makeProfile()
    {
        StudioRoomView::Profile p;
        p.studioName  = "MC1 GRANITE HALL STUDIOS A";
        p.tagline     = "Converted machine works — massive rock tracking room";
        p.wallColor   = QColor(0x28, 0x2c, 0x32);   // industrial concrete gray
        p.accentColor = QColor(0xc0, 0xc0, 0xc8);   // chrome / steel
        p.hasChamber  = true;
        p.sourceNames = {"Drum Riser", "Gtr Amp L", "Gtr Amp R", "Vocal Booth", "Bass DI"};
        p.micNames    = {"U67", "U47 fet", "SM57", "AKG D12", "Coles 4038", "RCA 44"};
        p.roomWidthScale = 380;
        return p;
    }
};
