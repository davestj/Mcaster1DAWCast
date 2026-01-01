// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ShowNotesEditor.h"

#include <QTextEdit>
#include <QVBoxLayout>

namespace dawcast {

ShowNotesEditor::ShowNotesEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setAcceptRichText(true);
    layout->addWidget(m_textEdit);
}

ShowNotesEditor::~ShowNotesEditor() = default;

void ShowNotesEditor::setHtml(const QString &html)
{
    m_textEdit->setHtml(html);
}

QString ShowNotesEditor::html() const
{
    return m_textEdit->toHtml();
}

void ShowNotesEditor::setPlainText(const QString &text)
{
    m_textEdit->setPlainText(text);
}

QString ShowNotesEditor::plainText() const
{
    return m_textEdit->toPlainText();
}

QString ShowNotesEditor::exportMarkdown() const
{
    // TODO: Convert rich text (QTextDocument) to Markdown
    // For now, return plain text as a basic fallback
    return m_textEdit->toPlainText();
}

} // namespace dawcast
