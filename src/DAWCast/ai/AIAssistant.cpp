// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AIAssistant.h"
#include "AIEngine.h"
#include "TranscriptionEngine.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace dawcast::ai {

// Request-ID prefixes to route responses back to the right signal
static const QString kShowNotes  = QStringLiteral("shownotes");
static const QString kTitle      = QStringLiteral("title");
static const QString kChapters   = QStringLiteral("chapters");
static const QString kTags       = QStringLiteral("tags");
static const QString kMix        = QStringLiteral("mix");
static const QString kAnalysis   = QStringLiteral("analysis");
static const QString kBatchTag   = QStringLiteral("batchtag");

AIAssistant::AIAssistant(QObject* parent)
    : QObject(parent)
    , m_engine(AIEngine::instance())
    , m_transcriber(new TranscriptionEngine(this))
{
    connect(m_engine, &AIEngine::textGenerated,
            this, &AIAssistant::onTextGenerated);
    connect(m_engine, &AIEngine::generationError,
            this, &AIAssistant::onGenerationError);
}

AIAssistant::~AIAssistant() = default;

// ── Prompt Dispatch ────────────────────────────────────────────────────────

void AIAssistant::sendPrompt(const QString& requestId, const QString& prompt)
{
    if (!m_engine->isAvailable()) {
        emit error(tr("AI backend is not available. Configure a backend in "
                      "the AI panel settings."));
        return;
    }
    m_engine->generateText(prompt, requestId);
}

// ── Podcast Features ───────────────────────────────────────────────────────

void AIAssistant::generateShowNotes(const QString& transcript)
{
    QString prompt = QStringLiteral(
        "You are a professional podcast producer. Generate detailed show notes "
        "in Markdown from the following transcript.\n\n"
        "Include:\n"
        "- A concise summary (2-3 sentences)\n"
        "- Key topics discussed (bulleted list)\n"
        "- Notable timestamps (HH:MM:SS format if available)\n"
        "- Guest information (if any guests are identifiable)\n"
        "- Relevant links or resources mentioned\n\n"
        "Transcript:\n%1").arg(transcript);
    sendPrompt(kShowNotes, prompt);
}

void AIAssistant::generateEpisodeTitle(const QString& transcript)
{
    QString prompt = QStringLiteral(
        "You are a podcast marketing expert. Based on the following transcript, "
        "suggest 5 compelling episode titles. Each title should be:\n"
        "- Under 80 characters\n"
        "- Engaging and descriptive\n"
        "- SEO-friendly\n\n"
        "Return ONLY the titles, one per line, numbered 1-5.\n\n"
        "Transcript:\n%1").arg(transcript);
    sendPrompt(kTitle, prompt);
}

void AIAssistant::generateChapterMarkers(const QString& transcript)
{
    QString prompt = QStringLiteral(
        "You are a podcast editor. Analyze this transcript and identify natural "
        "chapter/topic boundaries.\n\n"
        "Return ONLY a list in this exact format, one per line:\n"
        "TIMESTAMP_SECONDS | Chapter Title\n\n"
        "For example:\n"
        "0 | Introduction\n"
        "125 | The Main Topic\n"
        "340 | Interview Segment\n\n"
        "Aim for 4-10 chapters. Use the timestamps from the transcript if "
        "available, otherwise estimate based on content flow.\n\n"
        "Transcript:\n%1").arg(transcript);
    sendPrompt(kChapters, prompt);
}

void AIAssistant::suggestTags(const QString& transcript)
{
    QString prompt = QStringLiteral(
        "You are a media librarian. Analyze the following transcript and suggest "
        "relevant tags.\n\n"
        "Return tags in these categories, one category per line:\n"
        "Genre: tag1, tag2\n"
        "Mood: tag1, tag2\n"
        "Topics: tag1, tag2, tag3\n"
        "Keywords: tag1, tag2, tag3, tag4\n\n"
        "Keep each tag to 1-3 words. Aim for 3-5 tags per category.\n\n"
        "Transcript:\n%1").arg(transcript);
    sendPrompt(kTags, prompt);
}

// ── Mixing Features ────────────────────────────────────────────────────────

void AIAssistant::suggestMixSettings(const QString& genreHint, int trackCount)
{
    QString prompt = QStringLiteral(
        "You are an experienced audio engineer and mixing consultant. "
        "For a %1 project with %2 tracks, suggest detailed mix settings.\n\n"
        "Include recommendations for:\n"
        "- EQ: frequency ranges to boost/cut per typical track type\n"
        "- Compression: ratio, threshold, attack/release for each track type\n"
        "- Reverb: type and amount (dry/wet percentage)\n"
        "- Pan positions: stereo placement strategy\n"
        "- Volume balance: relative levels between track types\n"
        "- Master bus processing: limiter settings, final loudness target (LUFS)\n\n"
        "Be specific with numbers. Format clearly with sections and bullet points.")
        .arg(genreHint)
        .arg(trackCount);
    sendPrompt(kMix, prompt);
}

void AIAssistant::analyzeAudioIssues(const QString& audioDescription)
{
    QString prompt = QStringLiteral(
        "You are an audio troubleshooting expert. A user describes the following "
        "audio problem:\n\n\"%1\"\n\n"
        "Provide:\n"
        "1. Most likely cause(s)\n"
        "2. Step-by-step remediation using standard DAW tools "
        "(EQ, compression, noise gate, de-esser, etc.)\n"
        "3. Specific parameter suggestions (frequencies, ratios, thresholds)\n"
        "4. Prevention tips for future recordings").arg(audioDescription);
    sendPrompt(kAnalysis, prompt);
}

// ── Batch Tagging ──────────────────────────────────────────────────────────

void AIAssistant::autoTagFromContent(const QStringList& filePaths)
{
    if (filePaths.isEmpty()) {
        emit error(tr("No files selected for auto-tagging."));
        return;
    }
    m_batchFiles = filePaths;
    m_batchIndex = 0;
    emit batchTagProgress(0, m_batchFiles.size());

    // Kick off the first file — subsequent files are chained in
    // onTextGenerated() when a kBatchTag response arrives.
    QString prompt = QStringLiteral(
        "Based on the filename alone, suggest genre, mood, and keyword tags "
        "for this audio file. Return as comma-separated tags on a single line.\n\n"
        "Filename: %1").arg(QFileInfo(filePaths.first()).fileName());
    sendPrompt(kBatchTag, prompt);
}

// ── Response Routing ───────────────────────────────────────────────────────

void AIAssistant::onTextGenerated(const QString& requestId, const QString& result)
{
    if (requestId == kShowNotes) {
        emit showNotesReady(result);
    } else if (requestId == kTitle) {
        emit titleSuggestionReady(result);
    } else if (requestId == kChapters) {
        emit chaptersReady(parseChapters(result));
    } else if (requestId == kTags) {
        emit tagsReady(parseTags(result));
    } else if (requestId == kMix) {
        emit mixSuggestionsReady(result);
    } else if (requestId == kAnalysis) {
        emit audioAnalysisReady(result);
    } else if (requestId == kBatchTag) {
        // Emit tags for current file
        emit tagsReady(parseTags(result));
        m_batchIndex++;
        emit batchTagProgress(m_batchIndex, m_batchFiles.size());

        if (m_batchIndex < m_batchFiles.size()) {
            // Process next file
            QString prompt = QStringLiteral(
                "Based on the filename alone, suggest genre, mood, and keyword "
                "tags for this audio file. Return as comma-separated tags on a "
                "single line.\n\nFilename: %1")
                .arg(QFileInfo(m_batchFiles[m_batchIndex]).fileName());
            sendPrompt(kBatchTag, prompt);
        } else {
            emit batchTagComplete();
        }
    }
}

void AIAssistant::onGenerationError(const QString& requestId, const QString& err)
{
    Q_UNUSED(requestId)
    emit error(err);
}

// ── Response Parsers ───────────────────────────────────────────────────────

QList<QPair<qint64, QString>> AIAssistant::parseChapters(const QString& raw) const
{
    QList<QPair<qint64, QString>> chapters;

    // Expected format: "SECONDS | Title" or "SECONDS - Title"
    static QRegularExpression rx(
        QStringLiteral("^\\s*(\\d+)\\s*[|\\-]\\s*(.+)$"),
        QRegularExpression::MultilineOption);

    QRegularExpressionMatchIterator it = rx.globalMatch(raw);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        qint64 ms = m.captured(1).toLongLong() * 1000;   // seconds -> ms
        QString title = m.captured(2).trimmed();
        chapters.append({ms, title});
    }

    return chapters;
}

QStringList AIAssistant::parseTags(const QString& raw) const
{
    QStringList tags;

    // Collect comma-separated items, stripping category labels if present
    // Handles lines like "Genre: rock, alternative" or plain "rock, alternative"
    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString cleaned = line;
        // Remove leading category label (e.g. "Genre: ")
        int colon = cleaned.indexOf(QLatin1Char(':'));
        if (colon >= 0 && colon < 20)
            cleaned = cleaned.mid(colon + 1);

        const QStringList parts = cleaned.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& p : parts) {
            QString tag = p.trimmed();
            if (!tag.isEmpty())
                tags << tag;
        }
    }

    return tags;
}

} // namespace dawcast::ai
