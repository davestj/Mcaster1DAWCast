// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ScriptReaderPanel.h"

#include <QTextEdit>
#include <QToolBar>
#include <QLabel>
#include <QSpinBox>
#include <QAction>
#include <QTimer>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QFont>
#include <QScrollBar>
#include <QRegularExpression>

namespace dawcast::widgets {

// ── Style Constants ────────────────────────────────────────────────────────

static const QColor kReadColor(100, 110, 115);         // Dimmed read text
static const QColor kUnreadColor(220, 225, 230);       // Bright unread text
static const QColor kActiveColor(0, 220, 210);         // Teal active sentence
static const QColor kActiveBg(0, 188, 180, 30);        // Subtle teal highlight

// ── Construction ───────────────────────────────────────────────────────────

ScriptReaderPanel::ScriptReaderPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    buildToolbar();
    layout->addWidget(m_toolbar);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setAcceptRichText(true);
    m_textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_textEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background-color: #1a2228;"
        "  color: %1;"
        "  border: none;"
        "  padding: 16px;"
        "  selection-background-color: rgba(0, 188, 180, 0.3);"
        "}").arg(kUnreadColor.name()));
    layout->addWidget(m_textEdit, 1);

    // Position indicator
    m_positionLabel = new QLabel(tr("No script loaded"), this);
    m_positionLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #7a8a90; font-size: 11px; padding: 4px 8px; "
        "background: #161e24; }"));
    layout->addWidget(m_positionLabel);

    // Auto-scroll timer — gentle periodic scroll to keep active sentence visible
    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(500);
    connect(m_scrollTimer, &QTimer::timeout,
            this, &ScriptReaderPanel::onAutoScrollTick);

    onFontSizeChanged(m_fontSize);
}

ScriptReaderPanel::~ScriptReaderPanel() = default;

// ── Toolbar ────────────────────────────────────────────────────────────────

void ScriptReaderPanel::buildToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #1e2a30; border-bottom: 1px solid #2a3a42; "
        "spacing: 4px; padding: 2px; }"));

    // Load script
    m_actLoad = m_toolbar->addAction(tr("Load Script..."));
    m_actLoad->setToolTip(tr("Open a text or RTF script file"));
    connect(m_actLoad, &QAction::triggered, this, &ScriptReaderPanel::onLoadScript);

    m_toolbar->addSeparator();

    // Font size
    auto* fontLabel = new QLabel(tr("  Size:"), m_toolbar);
    fontLabel->setStyleSheet(QStringLiteral("QLabel { color: #aab5ba; font-size: 11px; }"));
    m_toolbar->addWidget(fontLabel);

    m_fontSizeSpin = new QSpinBox(m_toolbar);
    m_fontSizeSpin->setRange(8, 72);
    m_fontSizeSpin->setValue(m_fontSize);
    m_fontSizeSpin->setSuffix(QStringLiteral(" pt"));
    m_fontSizeSpin->setFixedWidth(80);
    m_fontSizeSpin->setStyleSheet(QStringLiteral(
        "QSpinBox { background: #2a3a42; color: #dde3e5; border: 1px solid #3a4a52; "
        "border-radius: 3px; padding: 2px; }"));
    connect(m_fontSizeSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &ScriptReaderPanel::onFontSizeChanged);
    m_toolbar->addWidget(m_fontSizeSpin);

    m_toolbar->addSeparator();

    // Advance / Reset
    m_actAdvance = m_toolbar->addAction(tr("Next Sentence"));
    m_actAdvance->setShortcut(QKeySequence(Qt::Key_Space | Qt::CTRL));
    m_actAdvance->setToolTip(tr("Mark current sentence as read and advance"));
    connect(m_actAdvance, &QAction::triggered, this, &ScriptReaderPanel::onAdvance);

    m_actReset = m_toolbar->addAction(tr("Reset"));
    m_actReset->setToolTip(tr("Return to the beginning of the script"));
    connect(m_actReset, &QAction::triggered, this, &ScriptReaderPanel::onReset);

    m_toolbar->addSeparator();

    // Auto-scroll toggle
    m_actAutoScroll = m_toolbar->addAction(tr("Auto-Scroll"));
    m_actAutoScroll->setCheckable(true);
    m_actAutoScroll->setChecked(m_autoScroll);
    m_actAutoScroll->setToolTip(tr("Automatically scroll to follow the active sentence"));
    connect(m_actAutoScroll, &QAction::toggled, this, [this](bool on) {
        m_autoScroll = on;
        if (on)
            m_scrollTimer->start();
        else
            m_scrollTimer->stop();
    });
}

// ── Script Loading ─────────────────────────────────────────────────────────

void ScriptReaderPanel::onLoadScript()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Script"),
        QString(),
        tr("Text Files (*.txt);;Rich Text (*.rtf);;All Files (*)"));

    if (!path.isEmpty())
        loadScript(path);
}

bool ScriptReaderPanel::loadScript(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream stream(&file);
    QString text = stream.readAll();
    file.close();

    setScriptText(text);
    return true;
}

void ScriptReaderPanel::setScriptText(const QString& text)
{
    splitIntoSentences(text);
    m_currentIndex = 0;
    rebuildHighlights();

    if (m_autoScroll)
        m_scrollTimer->start();

    emit sentenceChanged(0);
}

// ── Sentence Splitting ─────────────────────────────────────────────────────

void ScriptReaderPanel::splitIntoSentences(const QString& text)
{
    m_sentences.clear();

    // Split on sentence-ending punctuation followed by whitespace, or on
    // paragraph breaks.  Keep the punctuation with the sentence.
    static const QRegularExpression sentenceRx(
        QStringLiteral("(?<=[.!?])\\s+|\\n\\s*\\n"));

    const QStringList raw = text.split(sentenceRx, Qt::SkipEmptyParts);
    for (const QString& s : raw) {
        QString trimmed = s.trimmed();
        if (!trimmed.isEmpty())
            m_sentences.append(trimmed);
    }
}

// ── Navigation ─────────────────────────────────────────────────────────────

int ScriptReaderPanel::currentSentence() const
{
    return m_currentIndex;
}

void ScriptReaderPanel::advanceSentence()
{
    if (m_currentIndex < m_sentences.size() - 1) {
        ++m_currentIndex;
        rebuildHighlights();
        emit sentenceChanged(m_currentIndex);
    }
}

void ScriptReaderPanel::resetPosition()
{
    m_currentIndex = 0;
    rebuildHighlights();
    emit sentenceChanged(0);
}

void ScriptReaderPanel::setAutoScroll(bool on)
{
    m_autoScroll = on;
    m_actAutoScroll->setChecked(on);
}

bool ScriptReaderPanel::isAutoScrollEnabled() const
{
    return m_autoScroll;
}

// ── Slots ──────────────────────────────────────────────────────────────────

void ScriptReaderPanel::onFontSizeChanged(int pt)
{
    m_fontSize = pt;
    QFont f = m_textEdit->font();
    f.setPointSize(pt);
    m_textEdit->setFont(f);

    // Re-render with new font
    if (!m_sentences.isEmpty())
        rebuildHighlights();
}

void ScriptReaderPanel::onAdvance()
{
    advanceSentence();
}

void ScriptReaderPanel::onReset()
{
    resetPosition();
    m_textEdit->verticalScrollBar()->setValue(0);
}

void ScriptReaderPanel::onAutoScrollTick()
{
    if (!m_autoScroll || m_sentences.isEmpty())
        return;

    // Scroll to ensure the active sentence block is visible
    QTextCursor cursor = m_textEdit->textCursor();
    m_textEdit->ensureCursorVisible();
}

// ── Highlight Rendering ────────────────────────────────────────────────────

void ScriptReaderPanel::rebuildHighlights()
{
    if (m_sentences.isEmpty()) {
        m_textEdit->clear();
        m_positionLabel->setText(tr("No script loaded"));
        return;
    }

    // Rebuild the entire document with per-sentence formatting
    m_textEdit->clear();
    QTextCursor cursor(m_textEdit->document());

    QTextCharFormat readFmt;
    readFmt.setForeground(kReadColor);

    QTextCharFormat activeFmt;
    activeFmt.setForeground(kActiveColor);
    activeFmt.setBackground(kActiveBg);
    activeFmt.setFontWeight(QFont::Bold);

    QTextCharFormat unreadFmt;
    unreadFmt.setForeground(kUnreadColor);

    for (int i = 0; i < m_sentences.size(); ++i) {
        if (i > 0)
            cursor.insertText(QStringLiteral("\n\n"));

        QTextCharFormat& fmt = (i < m_currentIndex) ? readFmt
                             : (i == m_currentIndex) ? activeFmt
                             : unreadFmt;

        cursor.insertText(m_sentences[i], fmt);
    }

    // Position the text cursor at the active sentence so auto-scroll
    // can find it
    cursor.movePosition(QTextCursor::Start);
    for (int i = 0; i < m_currentIndex && !cursor.atEnd(); ++i) {
        // Move past sentence text + the two newlines separator
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
    }
    m_textEdit->setTextCursor(cursor);

    if (m_autoScroll)
        m_textEdit->ensureCursorVisible();

    // Update position label
    m_positionLabel->setText(tr("Sentence %1 of %2")
        .arg(m_currentIndex + 1)
        .arg(m_sentences.size()));
}

} // namespace dawcast::widgets
