// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TrackHeaderPanel.h"
#include "TrackHeaderWidget.h"
#include "../timeline/Timeline.h"
#include "../timeline/AudioTrack.h"
#include "../timeline/VideoTrack.h"
#include "../timeline/MidiTrack.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QLabel>

namespace dawcast::widgets {

namespace {
// Default track colors (cycled through)
const QColor kTrackColors[] = {
    QColor(0, 180, 180),    // teal
    QColor(100, 140, 220),  // blue
    QColor(200, 120, 80),   // orange
    QColor(120, 180, 100),  // green
    QColor(180, 100, 180),  // purple
    QColor(200, 180, 60),   // gold
    QColor(80, 180, 160),   // seafoam
    QColor(200, 80, 120),   // rose
};
constexpr int kColorCount = sizeof(kTrackColors) / sizeof(kTrackColors[0]);
} // anonymous namespace

TrackHeaderPanel::TrackHeaderPanel(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(TrackHeaderWidget::kHeaderWidth);
    setStyleSheet(QStringLiteral("background-color: #1a1e30;"));

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── Title bar: "TRACKS" label + "+" button ────────────────────────
    auto* titleBar = new QWidget(this);
    titleBar->setFixedHeight(kRulerHeight);
    titleBar->setStyleSheet(QStringLiteral(
        "background-color: #1e2030; border-bottom: 1px solid #2a2e3e;"));

    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(8, 0, 4, 0);
    titleLayout->setSpacing(4);

    auto* titleLabel = new QLabel(tr("TRACKS"), titleBar);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8890a8; font-size: 10px; font-weight: bold;"
        " letter-spacing: 1px; }"));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto* addBtn = new QPushButton(QStringLiteral("+"), titleBar);
    addBtn->setFixedSize(22, 22);
    addBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #2e3248; color: #8890a8; border: 1px solid #3a3e55;"
        "  border-radius: 3px; font-size: 14px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #3a4060; color: #bbc; }"));
    titleLayout->addWidget(addBtn);

    connect(addBtn, &QPushButton::clicked, this, &TrackHeaderPanel::addTrackRequested);

    outerLayout->addWidget(titleBar);

    // ── Scrollable header stack ───────────────────────────────────────
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // synced externally
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"));

    m_headerStack = new QWidget;
    m_headerStack->setStyleSheet(QStringLiteral("background: transparent;"));
    m_stackLayout = new QVBoxLayout(m_headerStack);
    m_stackLayout->setContentsMargins(0, 0, 0, 0);
    m_stackLayout->setSpacing(0);
    m_stackLayout->addStretch();  // push headers to the top

    m_scrollArea->setWidget(m_headerStack);
    outerLayout->addWidget(m_scrollArea, 1);
}

TrackHeaderPanel::~TrackHeaderPanel() = default;

void TrackHeaderPanel::setTimeline(dawcast::Timeline* timeline)
{
    if (m_timeline) {
        disconnect(m_timeline, nullptr, this, nullptr);
    }

    m_timeline = timeline;

    if (m_timeline) {
        connect(m_timeline, &Timeline::trackAdded,
                this, &TrackHeaderPanel::rebuildHeaders);
        connect(m_timeline, &Timeline::trackRemoved,
                this, &TrackHeaderPanel::rebuildHeaders);
    }

    rebuildHeaders();
}

QScrollBar* TrackHeaderPanel::verticalScrollBar() const
{
    return m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
}

void TrackHeaderPanel::rebuildHeaders()
{
    // Remove existing headers
    for (auto* hdr : m_headers) {
        m_stackLayout->removeWidget(hdr);
        hdr->deleteLater();
    }
    m_headers.clear();

    if (!m_timeline)
        return;

    int trackCount = m_timeline->trackCount();

    // Remove old stretch item before adding headers
    QLayoutItem* stretch = m_stackLayout->takeAt(m_stackLayout->count() - 1);
    delete stretch;

    for (int i = 0; i < trackCount; ++i) {
        auto* hdr = new TrackHeaderWidget(m_headerStack);
        hdr->setTrackIndex(i);
        hdr->setTrackColor(kTrackColors[i % kColorCount]);

        // Determine track name from the model
        QObject* trackObj = m_timeline->track(i);
        QString name;
        if (auto* at = qobject_cast<AudioTrack*>(trackObj))
            name = at->name();
        else if (auto* vt = qobject_cast<VideoTrack*>(trackObj))
            name = vt->name();
        else if (auto* mt = qobject_cast<MidiTrack*>(trackObj))
            name = mt->name();
        if (name.isEmpty())
            name = tr("Track %1").arg(i + 1);
        hdr->setTrackName(name);

        // Forward signals
        connect(hdr, &TrackHeaderWidget::eqRequested,
                this, &TrackHeaderPanel::eqRequested);
        connect(hdr, &TrackHeaderWidget::settingsRequested,
                this, &TrackHeaderPanel::settingsRequested);

        m_stackLayout->addWidget(hdr);
        m_headers.append(hdr);
    }

    // Re-add stretch at the bottom
    m_stackLayout->addStretch();
}

} // namespace dawcast::widgets
