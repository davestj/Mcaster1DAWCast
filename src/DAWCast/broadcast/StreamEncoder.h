// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QAbstractSocket>

class QTcpSocket;
class QTimer;

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
    void setStreamName(const QString &name);
    void setStreamDescription(const QString &description);
    void setStreamGenre(const QString &genre);
    void setStreamUrl(const QString &url);
    void setPublic(bool isPublic);
    void setCodec(const QString &codec);
    void setBitrate(int bitrate);

    void startStreaming();
    void stopStreaming();
    bool isStreaming() const;

    /// Send encoded audio data to the Icecast server.
    /// Returns true if the data was successfully written to the socket.
    bool sendAudioData(const char *data, int size);

Q_SIGNALS:
    void connected();
    void disconnected();
    void error(const QString &message);
    void bytesSent(int64_t totalBytes);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket *m_socket{nullptr};
    QTimer     *m_reconnectTimer{nullptr};

    QString m_serverUrl;
    int     m_serverPort{8000};
    QString m_mountPoint;
    QString m_password;
    QString m_streamName;
    QString m_streamDescription;
    QString m_streamGenre;
    QString m_streamUrl;
    bool    m_isPublic{false};
    QString m_codec{QStringLiteral("mp3")};
    int     m_bitrate{128};
    bool    m_streaming{false};
    bool    m_authenticated{false};
    int64_t m_totalBytesSent{0};
};

} // namespace dawcast
