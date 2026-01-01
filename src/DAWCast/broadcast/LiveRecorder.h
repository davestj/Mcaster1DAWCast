// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace dawcast {

class LiveRecorder : public QObject
{
    Q_OBJECT

public:
    explicit LiveRecorder(QObject *parent = nullptr);
    ~LiveRecorder() override;

    void setInputDevice(int deviceIndex);
    void setInputChannels(int channels);

    void startRecording(const QString &outputPath);
    void stopRecording();
    bool isRecording() const;

    void setPunchIn(int64_t samplePosition);
    void setPunchOut(int64_t samplePosition);

Q_SIGNALS:
    void levelUpdate(float peakL, float peakR);
    void recordingStarted();
    void recordingStopped();

private:
    int m_deviceIndex{-1};
    int m_inputChannels{2};
    bool m_recording{false};
    QString m_outputPath;
    int64_t m_punchIn{-1};
    int64_t m_punchOut{-1};
};

} // namespace dawcast
