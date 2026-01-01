// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MetadataPanel.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPixmap>

namespace dawcast::widgets {

MetadataPanel::MetadataPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QFormLayout(this);

    m_titleEdit  = new QLineEdit(this);
    m_artistEdit = new QLineEdit(this);
    m_albumEdit  = new QLineEdit(this);
    m_genreEdit  = new QLineEdit(this);

    layout->addRow(tr("Title:"),  m_titleEdit);
    layout->addRow(tr("Artist:"), m_artistEdit);
    layout->addRow(tr("Album:"),  m_albumEdit);
    layout->addRow(tr("Genre:"),  m_genreEdit);

    m_artworkThumb = new QLabel(this);
    m_artworkThumb->setFixedSize(128, 128);
    m_artworkThumb->setAlignment(Qt::AlignCenter);
    m_artworkThumb->setFrameShape(QFrame::Box);
    m_artworkThumb->setText(tr("No Artwork"));
    layout->addRow(tr("Artwork:"), m_artworkThumb);
}

MetadataPanel::~MetadataPanel() = default;

void MetadataPanel::setMetadataEditor(podcast::MetadataEditor* editor)
{
    m_editor = editor;
    // TODO: populate fields from editor
}

} // namespace dawcast::widgets
