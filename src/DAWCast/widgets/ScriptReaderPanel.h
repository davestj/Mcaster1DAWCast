// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>

class QTextEdit;
class QToolBar;
class QLabel;
class QSpinBox;
class QAction;
class QTimer;

namespace dawcast::widgets {

/// Voice Over mode panel — displays a script with sentence-level highlighting,
/// adjustable font size, and auto-scroll support during recording.
///
/// Features:
///   - Load plain text / RTF scripts from disk
///   - Adjustable font size (8 pt ... 72 pt)
///   - Split text into sentences for read/unread tracking
///   - Auto-scroll follows the active sentence during recording
///   - Mark-as-read button to advance the cursor manually
///   - Clear visual distinction between read (dimmed) and unread sentences
class ScriptReaderPanel : public QWidget {
    Q_OBJECT

public:
    explicit ScriptReaderPanel(QWidget* parent = nullptr);
    ~ScriptReaderPanel() override;

    /// Load a script file (plain text or rich text).
    bool loadScript(const QString& filePath);

    /// Set raw script text programmatically.
    void setScriptText(const QString& text);

    /// Current active sentence index (0-based).
    int currentSentence() const;

    /// Advance to the next sentence and re-highlight.
    void advanceSentence();

    /// Reset to the beginning.
    void resetPosition();

    /// Enable/disable auto-scroll when the playhead advances.
    void setAutoScroll(bool on);
    bool isAutoScrollEnabled() const;

signals:
    /// Emitted when the active sentence changes.
    void sentenceChanged(int index);

    /// Emitted when the user requests a script file load via the toolbar.
    void loadRequested();

private slots:
    void onLoadScript();
    void onFontSizeChanged(int pt);
    void onAdvance();
    void onReset();
    void onAutoScrollTick();

private:
    void buildToolbar();
    void rebuildHighlights();
    void splitIntoSentences(const QString& text);

    QToolBar*   m_toolbar       = nullptr;
    QTextEdit*  m_textEdit      = nullptr;
    QSpinBox*   m_fontSizeSpin  = nullptr;
    QLabel*     m_positionLabel = nullptr;

    QAction*    m_actLoad       = nullptr;
    QAction*    m_actAdvance    = nullptr;
    QAction*    m_actReset      = nullptr;
    QAction*    m_actAutoScroll = nullptr;

    QTimer*     m_scrollTimer   = nullptr;

    QStringList m_sentences;
    int         m_currentIndex  = 0;
    bool        m_autoScroll    = true;
    int         m_fontSize      = 18;
};

} // namespace dawcast::widgets
