// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MetadataPanel.h"
#include "MetadataEditor.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QFileDialog>
#include <QFileInfo>

namespace dawcast::widgets {

MetadataPanel::MetadataPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QFormLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Text fields
    m_titleEdit  = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(tr("Episode or track title"));
    layout->addRow(tr("Title:"), m_titleEdit);

    m_artistEdit = new QLineEdit(this);
    m_artistEdit->setPlaceholderText(tr("Artist or host name"));
    layout->addRow(tr("Artist:"), m_artistEdit);

    m_albumEdit  = new QLineEdit(this);
    m_albumEdit->setPlaceholderText(tr("Album or podcast name"));
    layout->addRow(tr("Album:"), m_albumEdit);

    m_genreEdit  = new QLineEdit(this);
    m_genreEdit->setPlaceholderText(tr("Genre or category"));
    layout->addRow(tr("Genre:"), m_genreEdit);

    // Artwork thumbnail + Browse button
    auto* artworkRow = new QHBoxLayout;

    m_artworkThumb = new QLabel(this);
    m_artworkThumb->setFixedSize(128, 128);
    m_artworkThumb->setAlignment(Qt::AlignCenter);
    m_artworkThumb->setFrameShape(QFrame::Box);
    m_artworkThumb->setStyleSheet(QStringLiteral(
        "QLabel { background: #1e1e24; border: 1px solid #555; color: #888; font-size: 11px; }"));
    m_artworkThumb->setText(tr("No Artwork"));
    artworkRow->addWidget(m_artworkThumb);

    auto* artworkBtnLayout = new QVBoxLayout;
    auto* browseBtn = new QPushButton(tr("Browse..."), this);
    auto* clearBtn  = new QPushButton(tr("Clear"), this);
    artworkBtnLayout->addWidget(browseBtn);
    artworkBtnLayout->addWidget(clearBtn);
    artworkBtnLayout->addStretch();
    artworkRow->addLayout(artworkBtnLayout);
    artworkRow->addStretch();

    layout->addRow(tr("Artwork:"), artworkRow);

    // Connect Browse button
    connect(browseBtn, &QPushButton::clicked, this, [this] {
        QString path = QFileDialog::getOpenFileName(
            this, tr("Select Artwork"),
            QString(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.gif);;All Files (*)"));
        if (!path.isEmpty()) {
            setArtworkPath(path);
        }
    });

    // Connect Clear button
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        m_artworkThumb->setPixmap(QPixmap());
        m_artworkThumb->setText(tr("No Artwork"));
        if (m_editor) {
            m_editor->setArtwork(QString());
        }
    });

    // Connect text field changes to MetadataEditor (deferred until editor is set)
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_editor) m_editor->setTitle(text);
    });
    connect(m_artistEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_editor) m_editor->setArtist(text);
    });
    connect(m_albumEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_editor) m_editor->setAlbum(text);
    });
    connect(m_genreEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_editor) m_editor->setGenre(text);
    });
}

MetadataPanel::~MetadataPanel() = default;

void MetadataPanel::setMetadataEditor(MetadataEditor* editor)
{
    m_editor = editor;

    if (!editor) return;

    // Populate fields from editor, blocking signals to avoid feedback loop
    m_titleEdit->blockSignals(true);
    m_titleEdit->setText(editor->title());
    m_titleEdit->blockSignals(false);

    m_artistEdit->blockSignals(true);
    m_artistEdit->setText(editor->artist());
    m_artistEdit->blockSignals(false);

    m_albumEdit->blockSignals(true);
    m_albumEdit->setText(editor->album());
    m_albumEdit->blockSignals(false);

    m_genreEdit->blockSignals(true);
    m_genreEdit->setText(editor->genre());
    m_genreEdit->blockSignals(false);

    // Load artwork if available
    QString artworkPath = editor->artwork();
    if (!artworkPath.isEmpty()) {
        setArtworkPath(artworkPath);
    }

    // Listen for external changes to metadata
    connect(editor, &MetadataEditor::metadataChanged, this, [this] {
        if (!m_editor) return;
        if (m_titleEdit->text() != m_editor->title()) {
            m_titleEdit->blockSignals(true);
            m_titleEdit->setText(m_editor->title());
            m_titleEdit->blockSignals(false);
        }
        if (m_artistEdit->text() != m_editor->artist()) {
            m_artistEdit->blockSignals(true);
            m_artistEdit->setText(m_editor->artist());
            m_artistEdit->blockSignals(false);
        }
        if (m_albumEdit->text() != m_editor->album()) {
            m_albumEdit->blockSignals(true);
            m_albumEdit->setText(m_editor->album());
            m_albumEdit->blockSignals(false);
        }
        if (m_genreEdit->text() != m_editor->genre()) {
            m_genreEdit->blockSignals(true);
            m_genreEdit->setText(m_editor->genre());
            m_genreEdit->blockSignals(false);
        }
    });
}

void MetadataPanel::setArtworkPath(const QString& path)
{
    QPixmap pixmap(path);
    if (!pixmap.isNull()) {
        m_artworkThumb->setPixmap(
            pixmap.scaled(m_artworkThumb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_artworkThumb->setText(QString());
        if (m_editor) {
            m_editor->setArtwork(path);
        }
    }
}

} // namespace dawcast::widgets
