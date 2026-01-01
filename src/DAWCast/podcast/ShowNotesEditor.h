// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QString>

class QTextEdit;

namespace dawcast {

class ShowNotesEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ShowNotesEditor(QWidget *parent = nullptr);
    ~ShowNotesEditor() override;

    void setHtml(const QString &html);
    QString html() const;

    void setPlainText(const QString &text);
    QString plainText() const;

    QString exportMarkdown() const;

private:
    QTextEdit *m_textEdit{nullptr};
};

} // namespace dawcast
