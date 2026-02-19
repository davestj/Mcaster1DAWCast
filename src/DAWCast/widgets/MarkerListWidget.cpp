// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MarkerListWidget.h"
#include "BevelButton.h"
#include "../timeline/Timeline.h"
#include "../timeline/Marker.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QColorDialog>
#include <QInputDialog>
#include <QLineEdit>

#include <algorithm>

namespace dawcast::widgets {

// Column indices
enum Column {
    ColColor   = 0,
    ColName    = 1,
    ColTime    = 2,
    ColType    = 3,
    ColComment = 4,
    ColCount   = 5
};

MarkerListWidget::MarkerListWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // ── Toolbar ────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(4);

    m_addBtn = new BevelButton(tr("+ Marker"), this);
    m_addBtn->setFixedHeight(24);
    m_addBtn->setToolTip(tr("Add marker at current playhead position"));
    toolbar->addWidget(m_addBtn);

    m_deleteBtn = new BevelButton(tr("Delete"), this);
    m_deleteBtn->setFixedHeight(24);
    m_deleteBtn->setToolTip(tr("Delete selected marker"));
    toolbar->addWidget(m_deleteBtn);

    toolbar->addSpacing(8);

    auto* sortLabel = new QLabel(tr("Sort:"), this);
    sortLabel->setStyleSheet(QStringLiteral("QLabel { color: #aaa; font-size: 11px; }"));
    toolbar->addWidget(sortLabel);

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(tr("By Time"));
    m_sortCombo->addItem(tr("By Name"));
    m_sortCombo->setFixedWidth(80);
    toolbar->addWidget(m_sortCombo);

    toolbar->addStretch();
    layout->addLayout(toolbar);

    // ── Table ──────────────────────────────────────────────────────────
    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels({
        tr("Color"), tr("Name"), tr("Time"), tr("Type"), tr("Comment")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(ColColor, QHeaderView::Fixed);
    m_table->setColumnWidth(ColColor, 40);
    m_table->setColumnWidth(ColName, 120);
    m_table->setColumnWidth(ColTime, 110);
    m_table->setColumnWidth(ColType, 70);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget { background-color: #1e2230; color: #ccc; font-size: 11px; }"
        "QTableWidget::item:selected { background-color: #3a4a6a; }"
        "QHeaderView::section { background-color: #252838; color: #aaa; "
        "  padding: 3px; border: 1px solid #333; font-size: 10px; }"));
    layout->addWidget(m_table);

    // ── Bottom ─────────────────────────────────────────────────────────
    m_countLabel = new QLabel(tr("0 markers"), this);
    m_countLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #888; font-size: 10px; }"));
    layout->addWidget(m_countLabel);

    // ── Connections ────────────────────────────────────────────────────
    connect(m_addBtn, &BevelButton::clicked, this, &MarkerListWidget::onAddMarker);
    connect(m_deleteBtn, &BevelButton::clicked, this, &MarkerListWidget::onDeleteMarker);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MarkerListWidget::onSortChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &MarkerListWidget::onCellDoubleClicked);
    connect(m_table, &QTableWidget::cellChanged,
            this, &MarkerListWidget::onCellChanged);
    connect(m_table, &QTableWidget::customContextMenuRequested,
            this, &MarkerListWidget::onCustomContextMenu);
}

MarkerListWidget::~MarkerListWidget() = default;

void MarkerListWidget::setTimeline(Timeline* timeline)
{
    if (m_timeline) {
        disconnect(m_timeline, nullptr, this, nullptr);
    }
    m_timeline = timeline;
    if (m_timeline) {
        connect(m_timeline, &Timeline::markersChanged, this, &MarkerListWidget::refreshList);
    }
    refreshList();
}

QString MarkerListWidget::formatTimecode(int64_t samples) const
{
    int sampleRate = 48000;
    if (m_timeline) sampleRate = m_timeline->sampleRate();
    if (sampleRate <= 0) sampleRate = 48000;

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

void MarkerListWidget::populateRow(int row, int markerIndex)
{
    const auto& mkr = m_timeline->marker(markerIndex);

    // Color swatch (non-editable)
    auto* colorItem = new QTableWidgetItem();
    colorItem->setBackground(mkr.color());
    colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
    colorItem->setData(Qt::UserRole, markerIndex);
    m_table->setItem(row, ColColor, colorItem);

    // Name (editable)
    auto* nameItem = new QTableWidgetItem(mkr.name());
    nameItem->setData(Qt::UserRole, markerIndex);
    m_table->setItem(row, ColName, nameItem);

    // Time (non-editable)
    auto* timeItem = new QTableWidgetItem(formatTimecode(mkr.position()));
    timeItem->setFlags(timeItem->flags() & ~Qt::ItemIsEditable);
    timeItem->setData(Qt::UserRole, markerIndex);
    m_table->setItem(row, ColTime, timeItem);

    // Type (non-editable)
    auto* typeItem = new QTableWidgetItem(Marker::typeName(mkr.type()));
    typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
    typeItem->setData(Qt::UserRole, markerIndex);
    m_table->setItem(row, ColType, typeItem);

    // Comment (editable)
    auto* commentItem = new QTableWidgetItem(mkr.comment());
    commentItem->setData(Qt::UserRole, markerIndex);
    m_table->setItem(row, ColComment, commentItem);
}

void MarkerListWidget::refreshList()
{
    m_updatingTable = true;

    m_table->setRowCount(0);
    if (!m_timeline) {
        m_countLabel->setText(tr("0 markers"));
        m_updatingTable = false;
        return;
    }

    int count = m_timeline->markerCount();
    m_table->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        populateRow(i, i);
    }

    m_countLabel->setText(tr("%1 marker(s)").arg(count));
    m_updatingTable = false;
}

void MarkerListWidget::onAddMarker()
{
    if (!m_timeline) return;

    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Add Marker"),
                                         tr("Marker name:"),
                                         QLineEdit::Normal,
                                         QStringLiteral("Marker %1").arg(m_timeline->markerCount() + 1),
                                         &ok);
    if (!ok || name.isEmpty()) return;

    Marker mkr(name, m_timeline->playhead(), Marker::Type::Cue);
    m_timeline->addMarker(mkr);
}

void MarkerListWidget::onDeleteMarker()
{
    if (!m_timeline) return;

    int row = m_table->currentRow();
    if (row < 0) return;

    auto* item = m_table->item(row, ColColor);
    if (!item) return;
    int markerIndex = item->data(Qt::UserRole).toInt();

    m_timeline->removeMarker(markerIndex);
}

void MarkerListWidget::onSortChanged(int index)
{
    if (!m_timeline) return;

    if (index == 0) {
        // Sort by time (default)
        m_timeline->sortMarkers();
    } else {
        // Sort by name
        auto& markers = m_timeline->markers();
        std::sort(markers.begin(), markers.end(),
                  [](const Marker& a, const Marker& b) {
            return a.name().toLower() < b.name().toLower();
        });
        emit m_timeline->markersChanged();
    }
}

void MarkerListWidget::onCellDoubleClicked(int row, int column)
{
    Q_UNUSED(column)
    if (!m_timeline) return;

    auto* item = m_table->item(row, ColColor);
    if (!item) return;
    int markerIndex = item->data(Qt::UserRole).toInt();

    if (markerIndex >= 0 && markerIndex < m_timeline->markerCount()) {
        emit markerSelected(m_timeline->marker(markerIndex).position());
    }
}

void MarkerListWidget::onCellChanged(int row, int column)
{
    if (m_updatingTable || !m_timeline) return;

    auto* item = m_table->item(row, ColColor);
    if (!item) return;
    int markerIndex = item->data(Qt::UserRole).toInt();
    if (markerIndex < 0 || markerIndex >= m_timeline->markerCount()) return;

    Marker mkr = m_timeline->marker(markerIndex);

    if (column == ColName) {
        auto* nameItem = m_table->item(row, ColName);
        if (nameItem) mkr.setName(nameItem->text());
    } else if (column == ColComment) {
        auto* commentItem = m_table->item(row, ColComment);
        if (commentItem) mkr.setComment(commentItem->text());
    }

    m_updatingTable = true;
    m_timeline->setMarker(markerIndex, mkr);
    m_updatingTable = false;
}

void MarkerListWidget::onCustomContextMenu(const QPoint& pos)
{
    if (!m_timeline) return;

    int row = m_table->rowAt(pos.y());
    if (row < 0) return;

    auto* item = m_table->item(row, ColColor);
    if (!item) return;
    int markerIndex = item->data(Qt::UserRole).toInt();
    if (markerIndex < 0 || markerIndex >= m_timeline->markerCount()) return;

    QMenu menu(this);

    menu.addAction(tr("Edit Name..."), this, [this, markerIndex]() {
        Marker mkr = m_timeline->marker(markerIndex);
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Edit Marker"),
                                             tr("Name:"), QLineEdit::Normal,
                                             mkr.name(), &ok);
        if (ok && !name.isEmpty()) {
            mkr.setName(name);
            m_timeline->setMarker(markerIndex, mkr);
        }
    });

    menu.addAction(tr("Change Color..."), this, [this, markerIndex]() {
        Marker mkr = m_timeline->marker(markerIndex);
        QColor color = QColorDialog::getColor(mkr.color(), this, tr("Marker Color"));
        if (color.isValid()) {
            mkr.setColor(color);
            m_timeline->setMarker(markerIndex, mkr);
        }
    });

    menu.addSeparator();

    menu.addAction(tr("Set as Loop Start"), this, [this, markerIndex]() {
        m_timeline->setLoopStart(m_timeline->marker(markerIndex).position());
        if (!m_timeline->loopEnabled() && m_timeline->loopEnd() > m_timeline->loopStart()) {
            m_timeline->setLoopEnabled(true);
        }
    });

    menu.addAction(tr("Set as Loop End"), this, [this, markerIndex]() {
        m_timeline->setLoopEnd(m_timeline->marker(markerIndex).position());
        if (!m_timeline->loopEnabled() && m_timeline->loopEnd() > m_timeline->loopStart()) {
            m_timeline->setLoopEnabled(true);
        }
    });

    menu.addSeparator();

    menu.addAction(tr("Delete"), this, [this, markerIndex]() {
        m_timeline->removeMarker(markerIndex);
    });

    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

} // namespace dawcast::widgets
