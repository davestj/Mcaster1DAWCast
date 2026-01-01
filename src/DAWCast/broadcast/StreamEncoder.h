// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace dawcast {

class StreamEncoder : public QObject
{
    Q_OBJECT

public:
    explicit StreamEncoder(QObject *parent = nullptr);
    ~StreamEncoder() override;

    void setServer(const QString &url, int port);
    void setMountPoint(const QString &mount);
    void setPassword(const QString &password);
    void setCodec(const QString &codec);
    void setBitrate(int bitrate);

    void startStreaming();
    void stopStreaming();
    bool isStreaming() const;

Q_SIGNALS:
    void connected();
    void disconnected();
    void error(const QString &message);
    void bytesSent(int64_t totalBytes);

private:
    QString m_serverUrl;
    int m_serverPort{8000};
    QString m_mountPoint;
    QString m_password;
    QString m_codec{"mp3"};
    int m_bitrate{128};
    bool m_streaming{false};
    int64_t m_totalBytesSent{0};
};

} // namespace dawcast
