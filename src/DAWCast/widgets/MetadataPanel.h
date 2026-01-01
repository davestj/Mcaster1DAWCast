// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>

namespace dawcast::podcast { class MetadataEditor; }

namespace dawcast::widgets {

class MetadataPanel : public QWidget {
    Q_OBJECT

public:
    explicit MetadataPanel(QWidget* parent = nullptr);
    ~MetadataPanel() override;

    void setMetadataEditor(podcast::MetadataEditor* editor);

private:
    podcast::MetadataEditor* m_editor = nullptr;

    QLineEdit* m_titleEdit   = nullptr;
    QLineEdit* m_artistEdit  = nullptr;
    QLineEdit* m_albumEdit   = nullptr;
    QLineEdit* m_genreEdit   = nullptr;
    QLabel*    m_artworkThumb = nullptr;
};

} // namespace dawcast::widgets
