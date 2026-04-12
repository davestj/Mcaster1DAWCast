// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TranscriptionEngine.h"
#include "../config/AppConfig.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>

namespace dawcast::ai {

TranscriptionEngine::TranscriptionEngine(QObject* parent)
    : QObject(parent)
{
    // Try common install locations for whisper-cpp / whisper
    static const QStringList defaultPaths = {
        QStringLiteral("/usr/local/bin/whisper-cpp"),
        QStringLiteral("/opt/homebrew/bin/whisper-cpp"),
        QStringLiteral("/usr/local/bin/whisper"),
        QStringLiteral("/opt/homebrew/bin/whisper"),
    };
    for (const QString& p : defaultPaths) {
        if (QFileInfo::exists(p)) {
            m_whisperPath = p;
            break;
        }
    }
}

TranscriptionEngine::~TranscriptionEngine()
{
    cancel();
}

// ── Configuration ──────────────────────────────────────────────────────────

void TranscriptionEngine::setConfig(const Config& config) { m_config = config; }
TranscriptionEngine::Config TranscriptionEngine::config() const { return m_config; }

void    TranscriptionEngine::setWhisperPath(const QString& p) { m_whisperPath = p; }
QString TranscriptionEngine::whisperPath() const              { return m_whisperPath; }

// ── Availability ───────────────────────────────────────────────────────────

bool TranscriptionEngine::isAvailable() const
{
    if (m_whisperPath.isEmpty())
        return false;
    QFileInfo fi(m_whisperPath);
    return fi.exists() && fi.isExecutable();
}

bool TranscriptionEngine::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

// ── Model Discovery ────────────────────────────────────────────────────────

QString TranscriptionEngine::defaultModelDir()
{
    return dawcast::config::AppConfig::whisperModelsDir();
}

QStringList TranscriptionEngine::availableModels()
{
    QDir dir(defaultModelDir());
    if (!dir.exists())
        return {};
    QStringList filters;
    filters << QStringLiteral("*.bin");
    QStringList models;
    for (const QFileInfo& fi : dir.entryInfoList(filters, QDir::Files, QDir::Name))
        models << fi.absoluteFilePath();
    return models;
}

// ── Transcription ──────────────────────────────────────────────────────────

QString TranscriptionEngine::formatExtension(Config::OutputFormat fmt)
{
    switch (fmt) {
    case Config::SRT:  return QStringLiteral(".srt");
    case Config::VTT:  return QStringLiteral(".vtt");
    case Config::Text: return QStringLiteral(".txt");
    case Config::JSON: return QStringLiteral(".json");
    }
    return QStringLiteral(".srt");
}

QStringList TranscriptionEngine::buildArgs(const QString& audioPath,
                                           const QString& outputPath) const
{
    QStringList args;

    // Model
    if (!m_config.modelPath.isEmpty()) {
        args << QStringLiteral("-m") << m_config.modelPath;
    }

    // Input file
    args << QStringLiteral("-f") << audioPath;

    // Output format flag
    switch (m_config.format) {
    case Config::SRT:  args << QStringLiteral("-osrt");  break;
    case Config::VTT:  args << QStringLiteral("-ovtt");  break;
    case Config::Text: args << QStringLiteral("-otxt");  break;
    case Config::JSON: args << QStringLiteral("-ojf");   break;
    }

    // Output file (without extension — whisper adds it)
    // Strip the format extension to get the base
    QString base = outputPath;
    QString ext  = formatExtension(m_config.format);
    if (base.endsWith(ext))
        base.chop(ext.size());
    args << QStringLiteral("-of") << base;

    // Language
    if (!m_config.language.isEmpty()) {
        args << QStringLiteral("-l") << m_config.language;
    }

    // Translate to English
    if (m_config.translateToEnglish)
        args << QStringLiteral("--translate");

    // Print progress (some whisper builds support this)
    args << QStringLiteral("--print-progress");

    return args;
}

void TranscriptionEngine::transcribe(const QString& audioPath,
                                     const QString& outputPath)
{
    if (!isAvailable()) {
        emit error(tr("Whisper binary not found. Install whisper-cpp and "
                      "configure its path in AI settings."));
        return;
    }

    if (isRunning()) {
        emit error(tr("A transcription is already in progress."));
        return;
    }

    if (!QFileInfo::exists(audioPath)) {
        emit error(tr("Audio file not found: %1").arg(audioPath));
        return;
    }

    // Resolve output path
    if (outputPath.isEmpty()) {
        QFileInfo fi(audioPath);
        m_outputPath = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
                       + formatExtension(m_config.format);
    } else {
        m_outputPath = outputPath;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &TranscriptionEngine::onProcessOutput);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus /*status*/) {
        onProcessFinished(exitCode);
    });

    QStringList args = buildArgs(audioPath, m_outputPath);
    m_process->start(m_whisperPath, args);
}

void TranscriptionEngine::cancel()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}

// ── Process Slots ──────────────────────────────────────────────────────────

void TranscriptionEngine::onProcessOutput()
{
    if (!m_process) return;

    // whisper-cpp prints progress lines like "whisper_print_progress: progress = 42%"
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data);

    static QRegularExpression rx(QStringLiteral("progress\\s*=\\s*(\\d+)%"));
    QRegularExpressionMatch match = rx.match(output);
    if (match.hasMatch()) {
        int pct = match.captured(1).toInt();
        emit progress(pct);
    }
}

void TranscriptionEngine::onProcessFinished(int exitCode)
{
    if (!m_process) return;

    if (exitCode != 0) {
        QString stderr_text = QString::fromUtf8(m_process->readAllStandardError());
        emit error(tr("Whisper exited with code %1: %2")
                       .arg(exitCode).arg(stderr_text.trimmed()));
    } else {
        // Read the generated output file
        QFile f(m_outputPath);
        QString text;
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            text = QString::fromUtf8(f.readAll());
            f.close();
        }
        emit progress(100);
        emit transcriptionComplete(m_outputPath, text);
    }

    m_process->deleteLater();
    m_process = nullptr;
}

} // namespace dawcast::ai
