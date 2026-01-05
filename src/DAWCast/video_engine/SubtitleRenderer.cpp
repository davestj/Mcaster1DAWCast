// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SubtitleRenderer.h"
#include <QPainter>
#include <QFont>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

namespace dawcast {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Parse an SRT/VTT timestamp string into seconds.
/// Accepts "HH:MM:SS,mmm" (SRT) and "HH:MM:SS.mmm" (VTT).
static double parseTimestamp(const QString& ts)
{
    // Normalise comma to period for uniform parsing
    QString s = ts.trimmed().replace(',', '.');

    static const QRegularExpression re(
        R"((\d+):(\d+):(\d+)\.(\d+))");
    QRegularExpressionMatch m = re.match(s);
    if (!m.hasMatch())
        return 0.0;

    double hours   = m.captured(1).toDouble();
    double minutes = m.captured(2).toDouble();
    double seconds = m.captured(3).toDouble();
    double millis  = m.captured(4).leftJustified(3, '0').left(3).toDouble();

    return hours * 3600.0 + minutes * 60.0 + seconds + millis / 1000.0;
}

/// Strip basic HTML-like tags (<b>, <i>, <u>, <font ...>) from subtitle text.
static QString stripTags(const QString& text)
{
    static const QRegularExpression tagRe(QStringLiteral("<[^>]+>"));
    QString result = text;
    result.remove(tagRe);
    return result;
}

// ---------------------------------------------------------------------------
// SubtitleRenderer
// ---------------------------------------------------------------------------

SubtitleRenderer::SubtitleRenderer(QObject* parent)
    : QObject(parent)
{
}

SubtitleRenderer::~SubtitleRenderer() = default;

bool SubtitleRenderer::loadSRT(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SubtitleRenderer: cannot open SRT file:" << path;
        return false;
    }

    m_entries.clear();
    QTextStream in(&file);

    // SRT format:
    //   <index>\n
    //   HH:MM:SS,mmm --> HH:MM:SS,mmm\n
    //   <text line(s)>\n
    //   <blank line>

    static const QRegularExpression arrowRe(
        R"((\d{2}:\d{2}:\d{2}[,\.]\d{3})\s*-->\s*(\d{2}:\d{2}:\d{2}[,\.]\d{3}))");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Skip empty lines and pure numeric indices
        if (line.isEmpty())
            continue;

        // Look for the timestamp line
        QRegularExpressionMatch tm = arrowRe.match(line);
        if (!tm.hasMatch()) {
            // Could be an index number -- skip and continue
            continue;
        }

        SubtitleEntry entry;
        entry.startTime = parseTimestamp(tm.captured(1));
        entry.endTime   = parseTimestamp(tm.captured(2));

        // Collect all text lines until the next blank line
        QString textBlock;
        while (!in.atEnd()) {
            line = in.readLine();
            if (line.trimmed().isEmpty())
                break;
            if (!textBlock.isEmpty())
                textBlock += '\n';
            textBlock += stripTags(line.trimmed());
        }
        entry.text = textBlock;

        if (!entry.text.isEmpty())
            m_entries.append(entry);
    }

    qDebug() << "SubtitleRenderer: loaded" << m_entries.size() << "SRT entries from" << path;
    return !m_entries.isEmpty();
}

bool SubtitleRenderer::loadVTT(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SubtitleRenderer: cannot open VTT file:" << path;
        return false;
    }

    m_entries.clear();
    QTextStream in(&file);

    // First line must be "WEBVTT" (possibly with additional metadata)
    QString header = in.readLine().trimmed();
    if (!header.startsWith("WEBVTT")) {
        qWarning() << "SubtitleRenderer: not a valid WebVTT file:" << path;
        return false;
    }

    // Skip header block (until first blank line)
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            break;
    }

    static const QRegularExpression arrowRe(
        R"((\d{2}:\d{2}:\d{2}[\.,]\d{3})\s*-->\s*(\d{2}:\d{2}:\d{2}[\.,]\d{3}))");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // Check if this line is a timestamp line
        QRegularExpressionMatch tm = arrowRe.match(line);
        if (!tm.hasMatch()) {
            // Could be a cue identifier -- read next line for timestamp
            line = in.readLine().trimmed();
            tm = arrowRe.match(line);
            if (!tm.hasMatch())
                continue;
        }

        SubtitleEntry entry;
        entry.startTime = parseTimestamp(tm.captured(1));
        entry.endTime   = parseTimestamp(tm.captured(2));

        QString textBlock;
        while (!in.atEnd()) {
            line = in.readLine();
            if (line.trimmed().isEmpty())
                break;
            if (!textBlock.isEmpty())
                textBlock += '\n';
            textBlock += stripTags(line.trimmed());
        }
        entry.text = textBlock;

        if (!entry.text.isEmpty())
            m_entries.append(entry);
    }

    qDebug() << "SubtitleRenderer: loaded" << m_entries.size() << "VTT entries from" << path;
    return !m_entries.isEmpty();
}

bool SubtitleRenderer::loadASS(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SubtitleRenderer: cannot open ASS file:" << path;
        return false;
    }

    m_entries.clear();
    QTextStream in(&file);

    // ASS format: look for [Events] section, then parse Dialogue lines
    // Dialogue: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
    bool inEvents = false;
    int startIdx  = -1;
    int endIdx    = -1;
    int textIdx   = -1;

    // Strip ASS override tags like {\b1}, {\an8}, {\pos(x,y)} etc.
    static const QRegularExpression overrideRe(QStringLiteral("\\{[^}]*\\}"));

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.startsWith('[')) {
            inEvents = (line.compare("[Events]", Qt::CaseInsensitive) == 0);
            continue;
        }

        if (!inEvents)
            continue;

        // Parse the Format line to find column indices
        if (line.startsWith("Format:", Qt::CaseInsensitive)) {
            QStringList cols = line.mid(7).split(',');
            for (int i = 0; i < cols.size(); ++i) {
                QString col = cols[i].trimmed().toLower();
                if (col == "start") startIdx = i;
                else if (col == "end") endIdx = i;
                else if (col == "text") textIdx = i;
            }
            continue;
        }

        // Parse Dialogue lines
        if (!line.startsWith("Dialogue:", Qt::CaseInsensitive))
            continue;

        if (startIdx < 0 || endIdx < 0 || textIdx < 0)
            continue;

        // Split only up to textIdx+1 fields (text field may contain commas)
        QString payload = line.mid(line.indexOf(':') + 1).trimmed();
        QStringList fields = payload.split(',');
        if (fields.size() <= textIdx)
            continue;

        // ASS timestamps: H:MM:SS.cc (centiseconds)
        auto parseASS = [](const QString& ts) -> double {
            static const QRegularExpression re(
                R"((\d+):(\d+):(\d+)\.(\d+))");
            QRegularExpressionMatch m = re.match(ts.trimmed());
            if (!m.hasMatch()) return 0.0;
            double h  = m.captured(1).toDouble();
            double mn = m.captured(2).toDouble();
            double s  = m.captured(3).toDouble();
            double cs = m.captured(4).leftJustified(2, '0').left(2).toDouble();
            return h * 3600.0 + mn * 60.0 + s + cs / 100.0;
        };

        SubtitleEntry entry;
        entry.startTime = parseASS(fields[startIdx]);
        entry.endTime   = parseASS(fields[endIdx]);

        // Text field is everything from textIdx onwards (may contain commas)
        QStringList textParts;
        for (int i = textIdx; i < fields.size(); ++i)
            textParts.append(fields[i]);
        QString text = textParts.join(',').trimmed();

        // Replace \N and \n with newlines, strip override tags
        text.replace("\\N", "\n");
        text.replace("\\n", "\n");
        text.remove(overrideRe);

        entry.text = text;
        if (!entry.text.isEmpty())
            m_entries.append(entry);
    }

    qDebug() << "SubtitleRenderer: loaded" << m_entries.size() << "ASS entries from" << path;
    return !m_entries.isEmpty();
}

void SubtitleRenderer::renderAt(QImage& frame, double timeSeconds)
{
    if (m_entries.isEmpty()) return;

    // Find active subtitle entries at the given time
    QString activeText;
    for (const auto& entry : m_entries) {
        if (timeSeconds >= entry.startTime && timeSeconds <= entry.endTime) {
            if (!activeText.isEmpty()) activeText += "\n";
            activeText += entry.text;
        }
    }

    if (activeText.isEmpty()) return;

    // Render subtitle text onto the frame with a semi-transparent background box
    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QFont font("Arial", 24, QFont::Bold);
    painter.setFont(font);

    QFontMetrics fm(font);
    QRect textBounds = fm.boundingRect(
        QRect(0, 0, frame.width() - 40, frame.height()),
        Qt::AlignHCenter | Qt::TextWordWrap,
        activeText);

    // Position the text near the bottom of the frame
    int margin = 20;
    int boxX = (frame.width() - textBounds.width()) / 2 - margin / 2;
    int boxY = frame.height() - textBounds.height() - margin * 3;
    int boxW = textBounds.width() + margin;
    int boxH = textBounds.height() + margin;

    // Draw semi-transparent background
    QRect bgRect(boxX, boxY, boxW, boxH);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 160));
    painter.drawRoundedRect(bgRect, 6, 6);

    // Draw text with white color and thin black outline for readability
    QRect textRect(boxX + margin / 2, boxY + margin / 2,
                   textBounds.width(), textBounds.height());

    // Outline pass
    painter.setPen(QColor(0, 0, 0, 200));
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            painter.drawText(textRect.translated(dx, dy),
                             Qt::AlignHCenter | Qt::TextWordWrap, activeText);
        }
    }

    // Main text
    painter.setPen(Qt::white);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::TextWordWrap, activeText);

    painter.end();
}

void SubtitleRenderer::clear()
{
    m_entries.clear();
}

} // namespace dawcast
