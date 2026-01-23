// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

namespace dawcast::ai {

/// Speech-to-text via Whisper.cpp (subprocess).
///
/// Calls the `whisper-cpp` CLI binary rather than linking the library,
/// keeping the build simple and isolated.  Transcription is fully async;
/// results arrive through signals.
class TranscriptionEngine : public QObject {
    Q_OBJECT

public:
    struct Config {
        QString modelPath;              ///< e.g. ~/.mcaster1/whisper-models/ggml-base.bin
        QString language  = QStringLiteral("en");
        bool    translateToEnglish = false;
        bool    timestamps         = true;

        enum OutputFormat { SRT, VTT, Text, JSON };
        OutputFormat format = SRT;
    };

    explicit TranscriptionEngine(QObject* parent = nullptr);
    ~TranscriptionEngine() override;

    void setConfig(const Config& config);
    Config config() const;

    /// Path to the whisper-cpp binary (e.g. /usr/local/bin/whisper-cpp).
    void    setWhisperPath(const QString& binaryPath);
    QString whisperPath() const;

    /// Transcribe an audio file asynchronously.
    /// If @p outputPath is empty, a sibling file with the format extension
    /// is written next to the source audio.
    void transcribe(const QString& audioPath,
                    const QString& outputPath = {});

    /// Cancel a running transcription.
    void cancel();

    /// True when the configured binary exists and is executable.
    bool isAvailable() const;

    /// True while a transcription job is in progress.
    bool isRunning() const;

    /// Scan the default model directory for available .bin files.
    static QStringList availableModels();

    /// Default directory for Whisper model files (~/.mcaster1/whisper-models/).
    static QString defaultModelDir();

signals:
    void progress(int percent);
    void transcriptionComplete(const QString& outputPath, const QString& text);
    void error(const QString& message);

private slots:
    void onProcessOutput();
    void onProcessFinished(int exitCode);

private:
    static QString formatExtension(Config::OutputFormat fmt);
    QStringList    buildArgs(const QString& audioPath,
                             const QString& outputPath) const;

    QProcess* m_process     = nullptr;
    Config    m_config;
    QString   m_whisperPath;
    QString   m_outputPath;         ///< Resolved output path for current job
};

} // namespace dawcast::ai
