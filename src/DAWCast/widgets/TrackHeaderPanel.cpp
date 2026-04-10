// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TrackHeaderPanel.h"
#include "TrackHeaderWidget.h"
#include "BevelButton.h"
#include "../timeline/Timeline.h"
#include "../timeline/AudioTrack.h"
#include "../timeline/VideoTrack.h"
#include "../timeline/MidiTrack.h"
#include "../timeline/TrackGroup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QApplication>

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
    addBtn->setToolTip(tr("Add a new audio track to the timeline"));
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
        connect(m_timeline, &Timeline::trackMoved,
                this, [this](int, int) { rebuildHeaders(); });
        connect(m_timeline, &Timeline::trackGroupAdded,
                this, &TrackHeaderPanel::rebuildHeaders);
        connect(m_timeline, &Timeline::trackGroupRemoved,
                this, &TrackHeaderPanel::rebuildHeaders);
        connect(m_timeline, &Timeline::trackGroupChanged,
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
    // Remove existing headers and group widgets
    for (auto* hdr : m_headers) {
        m_stackLayout->removeWidget(hdr);
        hdr->deleteLater();
    }
    m_headers.clear();

    // Remove group header widgets
    for (auto* gw : m_groupWidgets) {
        m_stackLayout->removeWidget(gw);
        gw->deleteLater();
    }
    m_groupWidgets.clear();

    if (!m_timeline)
        return;

    int trackCount = m_timeline->trackCount();

    // Remove old stretch item before adding headers
    QLayoutItem* stretch = m_stackLayout->takeAt(m_stackLayout->count() - 1);
    delete stretch;

    // Collect group membership for each track
    QList<TrackGroup*> groups = m_timeline->trackGroups();

    // Build a set of tracks that belong to any group
    QSet<QObject*> groupedTracks;
    for (auto* group : groups) {
        for (auto* t : group->tracks()) {
            groupedTracks.insert(t);
        }
    }

    // Helper: create a group header widget
    auto createGroupHeader = [this](TrackGroup* group, int groupIdx) -> QWidget* {
        auto* groupBar = new QWidget(m_headerStack);
        groupBar->setFixedWidth(TrackHeaderWidget::kHeaderWidth);
        groupBar->setFixedHeight(28);
        groupBar->setStyleSheet(
            QStringLiteral("background-color: %1; border-bottom: 1px solid #2a2e3e;")
                .arg(group->color().darker(160).name()));

        auto* layout = new QHBoxLayout(groupBar);
        layout->setContentsMargins(6, 2, 6, 2);
        layout->setSpacing(4);

        // Expand/collapse arrow
        auto* arrowBtn = new QPushButton(
            group->isCollapsed() ? QStringLiteral("\xe2\x96\xb6")   // right triangle
                                 : QStringLiteral("\xe2\x96\xbc"),  // down triangle
            groupBar);
        arrowBtn->setFixedSize(20, 20);
        arrowBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; color: #ccc; border: none;"
            " font-size: 10px; } QPushButton:hover { color: #fff; }"));
        layout->addWidget(arrowBtn);

        connect(arrowBtn, &QPushButton::clicked, this, [this, group, groupIdx]() {
            group->setCollapsed(!group->isCollapsed());
            emit groupCollapseToggled(groupIdx, group->isCollapsed());
            rebuildHeaders();
        });

        // Color indicator
        auto* colorDot = new QWidget(groupBar);
        colorDot->setFixedSize(10, 10);
        colorDot->setStyleSheet(
            QStringLiteral("background-color: %1; border-radius: 2px;").arg(group->color().name()));
        layout->addWidget(colorDot);

        // Group name
        auto* nameLabel = new QLabel(group->name(), groupBar);
        nameLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #dde; font-size: 11px; font-weight: bold; }"));
        layout->addWidget(nameLabel, 1);

        // Mute button
        auto* muteBtn = new BevelButton(QStringLiteral("M"), groupBar);
        muteBtn->setCheckable(true);
        muteBtn->setChecked(group->isMuted());
        muteBtn->setFixedSize(22, 18);
        muteBtn->setCheckedFaceColor(QColor(200, 120, 30));
        layout->addWidget(muteBtn);
        connect(muteBtn, &BevelButton::toggled, group, &TrackGroup::setMuted);

        // Solo button
        auto* soloBtn = new BevelButton(QStringLiteral("S"), groupBar);
        soloBtn->setCheckable(true);
        soloBtn->setChecked(group->isSolo());
        soloBtn->setFixedSize(22, 18);
        soloBtn->setCheckedFaceColor(QColor(200, 190, 50));
        layout->addWidget(soloBtn);
        connect(soloBtn, &BevelButton::toggled, group, &TrackGroup::setSolo);

        // Track count badge
        auto* countLabel = new QLabel(
            QStringLiteral("(%1)").arg(group->trackCount()), groupBar);
        countLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #889; font-size: 9px; }"));
        layout->addWidget(countLabel);

        return groupBar;
    };

    // Helper to add a track header at a given index with optional indent
    auto addTrackHeader = [&](int i, bool indented) {
        auto* hdr = new TrackHeaderWidget(m_headerStack);
        hdr->setTrackIndex(i);
        hdr->setTrackColor(kTrackColors[i % kColorCount]);

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

        // Apply indent for grouped tracks
        if (indented) {
            hdr->setContentsMargins(12, 0, 0, 0);
        }

        connect(hdr, &TrackHeaderWidget::eqRequested,
                this, &TrackHeaderPanel::eqRequested);
        connect(hdr, &TrackHeaderWidget::settingsRequested,
                this, &TrackHeaderPanel::settingsRequested);
        connect(hdr, &TrackHeaderWidget::duplicateRequested,
                this, &TrackHeaderPanel::duplicateTrackRequested);
        connect(hdr, &TrackHeaderWidget::deleteRequested,
                this, &TrackHeaderPanel::deleteTrackRequested);

        m_stackLayout->addWidget(hdr);
        m_headers.append(hdr);
    };

    // First, render groups and their tracks
    for (int g = 0; g < groups.size(); ++g) {
        TrackGroup* group = groups[g];

        // Group header bar
        auto* groupWidget = createGroupHeader(group, g);
        m_stackLayout->addWidget(groupWidget);
        m_groupWidgets.append(groupWidget);

        if (group->isCollapsed()) {
            // Collapsed: show a thin summary row
            auto* collapsedRow = new QWidget(m_headerStack);
            collapsedRow->setFixedWidth(TrackHeaderWidget::kHeaderWidth);
            collapsedRow->setFixedHeight(16);
            collapsedRow->setStyleSheet(
                QStringLiteral("background-color: %1; border-bottom: 1px solid #2a2e3e;")
                    .arg(group->color().darker(200).name()));
            auto* cLayout = new QHBoxLayout(collapsedRow);
            cLayout->setContentsMargins(24, 0, 6, 0);
            auto* cLabel = new QLabel(
                tr("%1 tracks").arg(group->trackCount()), collapsedRow);
            cLabel->setStyleSheet(QStringLiteral(
                "QLabel { color: #667; font-size: 9px; font-style: italic; }"));
            cLayout->addWidget(cLabel);
            m_stackLayout->addWidget(collapsedRow);
            m_groupWidgets.append(collapsedRow);
        } else {
            // Expanded: show all child tracks with indent
            for (auto* trackObj : group->tracks()) {
                // Find the track index in the timeline
                for (int i = 0; i < trackCount; ++i) {
                    if (m_timeline->track(i) == trackObj) {
                        addTrackHeader(i, true);
                        break;
                    }
                }
            }
        }
    }

    // Then render ungrouped tracks
    for (int i = 0; i < trackCount; ++i) {
        QObject* trackObj = m_timeline->track(i);
        if (groupedTracks.contains(trackObj)) continue;

        addTrackHeader(i, false);
    }

    // Re-add stretch at the bottom
    m_stackLayout->addStretch();
}

// ── Track reorder via drag ──────────────────────────────────────────────────

int TrackHeaderPanel::headerIndexAtPos(const QPoint& pos) const
{
    // Map position to the scroll area contents
    QPoint mappedPos = m_scrollArea->widget()->mapFrom(this, pos);
    int y = mappedPos.y();

    for (int i = 0; i < m_headers.size(); ++i) {
        auto* hdr = m_headers[i];
        int top = hdr->y();
        int bot = top + hdr->height();
        if (y >= top && y < bot) {
            return hdr->trackIndex();
        }
    }
    return -1;
}

void TrackHeaderPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
        m_dragFromIndex = headerIndexAtPos(event->pos());
        // Emit a trackSelected signal so the effects rack (and anything
        // else that cares about the "current" track) can react. We do this
        // on press rather than release so the selection feedback is
        // instant even if the user then drags the header.
        if (m_dragFromIndex >= 0) {
            emit trackSelected(m_dragFromIndex);
        }
    }
    QWidget::mousePressEvent(event);
}

void TrackHeaderPanel::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton) || m_dragFromIndex < 0) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // Start dragging after a minimum distance
    if (!m_dragReorder) {
        if ((event->pos() - m_dragStartPos).manhattanLength()
            < QApplication::startDragDistance()) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        m_dragReorder = true;
        setCursor(Qt::ClosedHandCursor);
    }

    QWidget::mouseMoveEvent(event);
}

void TrackHeaderPanel::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragReorder && m_dragFromIndex >= 0) {
        int toIndex = headerIndexAtPos(event->pos());
        if (toIndex >= 0 && toIndex != m_dragFromIndex) {
            emit trackMoveRequested(m_dragFromIndex, toIndex);
        }
    }

    m_dragReorder = false;
    m_dragFromIndex = -1;
    setCursor(Qt::ArrowCursor);
    QWidget::mouseReleaseEvent(event);
}

} // namespace dawcast::widgets
