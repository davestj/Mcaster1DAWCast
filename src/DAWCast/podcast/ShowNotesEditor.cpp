// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ShowNotesEditor.h"

#include <QTextEdit>
#include <QVBoxLayout>
#include <QRegularExpression>

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
    QString html = m_textEdit->toHtml();

    // If there's no HTML markup at all, return plain text directly
    if (!html.contains(QLatin1Char('<'))) {
        return html;
    }

    QString md = html;

    // Remove the Qt-generated HTML document wrapper
    // Strip everything up to and including <body...>
    static const QRegularExpression bodyOpenRe(
        QStringLiteral(".*<body[^>]*>"), QRegularExpression::DotMatchesEverythingOption);
    md.replace(bodyOpenRe, QString());
    // Strip </body></html> and trailing content
    static const QRegularExpression bodyCloseRe(
        QStringLiteral("</body>.*"), QRegularExpression::DotMatchesEverythingOption);
    md.replace(bodyCloseRe, QString());

    // Convert headings: <h1>text</h1> -> # text
    for (int level = 6; level >= 1; --level) {
        QString hashes = QString(level, QLatin1Char('#'));
        QRegularExpression headingRe(
            QStringLiteral("<h%1[^>]*>(.*?)</h%1>").arg(level),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        md.replace(headingRe, QStringLiteral("\n%1 \\1\n").arg(hashes));
    }

    // Convert bold: <b>text</b> and <strong>text</strong> -> **text**
    static const QRegularExpression boldRe(
        QStringLiteral("<(?:b|strong)(?:\\s[^>]*)?>(.+?)</(?:b|strong)>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    md.replace(boldRe, QStringLiteral("**\\1**"));

    // Convert italic: <i>text</i> and <em>text</em> -> *text*
    static const QRegularExpression italicRe(
        QStringLiteral("<(?:i|em)(?:\\s[^>]*)?>(.+?)</(?:i|em)>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    md.replace(italicRe, QStringLiteral("*\\1*"));

    // Convert links: <a href="url">text</a> -> [text](url)
    static const QRegularExpression linkRe(
        QStringLiteral("<a\\s+[^>]*href=\"([^\"]*)\"[^>]*>(.*?)</a>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    md.replace(linkRe, QStringLiteral("[\\2](\\1)"));

    // Convert unordered list items: <li>text</li> -> - text
    static const QRegularExpression liRe(
        QStringLiteral("<li[^>]*>(.*?)</li>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    md.replace(liRe, QStringLiteral("- \\1"));

    // Remove list wrapper tags
    static const QRegularExpression ulOlRe(
        QStringLiteral("</?(?:ul|ol)[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    md.replace(ulOlRe, QString());

    // Convert paragraphs: <p>text</p> -> text\n\n
    static const QRegularExpression pOpenRe(
        QStringLiteral("<p[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    md.replace(pOpenRe, QString());
    static const QRegularExpression pCloseRe(
        QStringLiteral("</p>"),
        QRegularExpression::CaseInsensitiveOption);
    md.replace(pCloseRe, QStringLiteral("\n\n"));

    // Convert <br> and <br/> to newlines
    static const QRegularExpression brRe(
        QStringLiteral("<br\\s*/?>"),
        QRegularExpression::CaseInsensitiveOption);
    md.replace(brRe, QStringLiteral("\n"));

    // Strip any remaining HTML tags
    static const QRegularExpression allTagsRe(QStringLiteral("<[^>]+>"));
    md.replace(allTagsRe, QString());

    // Decode common HTML entities
    md.replace(QStringLiteral("&amp;"),  QStringLiteral("&"));
    md.replace(QStringLiteral("&lt;"),   QStringLiteral("<"));
    md.replace(QStringLiteral("&gt;"),   QStringLiteral(">"));
    md.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    md.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    md.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));

    // Clean up excessive blank lines (more than 2 consecutive newlines -> 2)
    static const QRegularExpression multiNewlineRe(QStringLiteral("\n{3,}"));
    md.replace(multiNewlineRe, QStringLiteral("\n\n"));

    return md.trimmed();
}

} // namespace dawcast
