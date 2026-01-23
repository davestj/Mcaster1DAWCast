// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPair>
#include <QList>

namespace dawcast::ai {

class AIEngine;
class TranscriptionEngine;

/// High-level podcast and mixing AI features built on AIEngine and
/// TranscriptionEngine.
///
/// Each method constructs a domain-specific prompt, sends it to the
/// configured LLM backend, and emits structured results via signals.
class AIAssistant : public QObject {
    Q_OBJECT

public:
    explicit AIAssistant(QObject* parent = nullptr);
    ~AIAssistant() override;

    // ── Podcast features ───────────────────────────────────────────────

    /// Generate professional show notes (markdown) from a transcript.
    void generateShowNotes(const QString& transcript);

    /// Suggest episode titles from a transcript.
    void generateEpisodeTitle(const QString& transcript);

    /// Auto-detect chapter/topic boundaries in a transcript.
    void generateChapterMarkers(const QString& transcript);

    /// Suggest genre, mood, and keyword tags from a transcript.
    void suggestTags(const QString& transcript);

    // ── Mixing features ────────────────────────────────────────────────

    /// Suggest EQ, compression, reverb, pan, and volume settings.
    void suggestMixSettings(const QString& genreHint, int trackCount);

    /// Describe audio problems and get analysis / remediation tips.
    void analyzeAudioIssues(const QString& audioDescription);

    // ── Batch ──────────────────────────────────────────────────────────

    /// Auto-tag files by analysing their content (via transcription + LLM).
    void autoTagFromContent(const QStringList& filePaths);

signals:
    void showNotesReady(const QString& markdown);
    void titleSuggestionReady(const QString& title);
    void chaptersReady(const QList<QPair<qint64, QString>>& chapters);
    void tagsReady(const QStringList& tags);
    void mixSuggestionsReady(const QString& suggestions);
    void audioAnalysisReady(const QString& analysis);
    void batchTagProgress(int current, int total);
    void batchTagComplete();
    void error(const QString& message);

private slots:
    void onTextGenerated(const QString& requestId, const QString& result);
    void onGenerationError(const QString& requestId, const QString& err);

private:
    void sendPrompt(const QString& requestId, const QString& prompt);
    QList<QPair<qint64, QString>> parseChapters(const QString& raw) const;
    QStringList parseTags(const QString& raw) const;

    AIEngine*            m_engine       = nullptr;
    TranscriptionEngine* m_transcriber  = nullptr;

    // Batch tagging state
    QStringList m_batchFiles;
    int         m_batchIndex = 0;
};

} // namespace dawcast::ai
