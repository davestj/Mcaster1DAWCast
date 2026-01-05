// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "StreamEncoder.h"

#include <QTcpSocket>
#include <QTimer>
#include <QDebug>

namespace dawcast {

StreamEncoder::StreamEncoder(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
{
    // Wire up socket signals
    connect(m_socket, &QTcpSocket::connected, this, &StreamEncoder::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &StreamEncoder::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &StreamEncoder::onSocketReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &StreamEncoder::onSocketError);

    // Reconnect timer (disabled by default)
    m_reconnectTimer->setInterval(5000);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_streaming && m_socket->state() == QAbstractSocket::UnconnectedState) {
            qDebug() << "StreamEncoder: Attempting reconnect to"
                     << m_serverUrl << ":" << m_serverPort;
            m_socket->connectToHost(m_serverUrl, static_cast<quint16>(m_serverPort));
        }
    });
}

StreamEncoder::~StreamEncoder()
{
    if (m_streaming) {
        stopStreaming();
    }
}

void StreamEncoder::setServer(const QString &url, int port)
{
    m_serverUrl = url;
    m_serverPort = port;
}

void StreamEncoder::setMountPoint(const QString &mount)
{
    m_mountPoint = mount;
}

void StreamEncoder::setPassword(const QString &password)
{
    m_password = password;
}

void StreamEncoder::setStreamName(const QString &name)
{
    m_streamName = name;
}

void StreamEncoder::setStreamDescription(const QString &description)
{
    m_streamDescription = description;
}

void StreamEncoder::setStreamGenre(const QString &genre)
{
    m_streamGenre = genre;
}

void StreamEncoder::setStreamUrl(const QString &url)
{
    m_streamUrl = url;
}

void StreamEncoder::setPublic(bool isPublic)
{
    m_isPublic = isPublic;
}

void StreamEncoder::setCodec(const QString &codec)
{
    m_codec = codec;
}

void StreamEncoder::setBitrate(int bitrate)
{
    m_bitrate = bitrate;
}

void StreamEncoder::startStreaming()
{
    if (m_streaming) {
        qWarning() << "StreamEncoder: Already streaming";
        return;
    }

    if (m_serverUrl.isEmpty()) {
        emit error(QStringLiteral("Server URL is not set"));
        return;
    }

    if (m_mountPoint.isEmpty()) {
        emit error(QStringLiteral("Mount point is not set"));
        return;
    }

    m_streaming = true;
    m_totalBytesSent = 0;
    m_authenticated = false;

    qDebug() << "StreamEncoder: Connecting to" << m_serverUrl << ":" << m_serverPort
             << "mount:" << m_mountPoint;

    m_socket->connectToHost(m_serverUrl, static_cast<quint16>(m_serverPort));
}

void StreamEncoder::stopStreaming()
{
    m_streaming = false;
    m_authenticated = false;
    m_reconnectTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->flush();
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(3000);
        }
    }

    emit disconnected();
}

bool StreamEncoder::isStreaming() const
{
    return m_streaming;
}

bool StreamEncoder::sendAudioData(const char *data, int size)
{
    if (!m_streaming || !m_authenticated) {
        return false;
    }

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    qint64 written = m_socket->write(data, size);
    if (written > 0) {
        m_totalBytesSent += written;
        emit bytesSent(m_totalBytesSent);
        return true;
    }

    return false;
}

// ─── Private Slots ─────────────────────────────────────────────────

void StreamEncoder::onSocketConnected()
{
    qDebug() << "StreamEncoder: TCP connected, sending SOURCE request";

    // Build the Icecast SOURCE HTTP request
    // Format: SOURCE /mountpoint HTTP/1.0
    //         Authorization: Basic <base64(source:password)>
    //         Content-Type: audio/mpeg (or audio/ogg, audio/aac)
    //         ice-name: ...
    //         ice-description: ...
    //         ice-public: 0|1

    // Determine content type from codec
    QString contentType;
    if (m_codec == QStringLiteral("mp3")) {
        contentType = QStringLiteral("audio/mpeg");
    } else if (m_codec == QStringLiteral("ogg") || m_codec == QStringLiteral("vorbis")) {
        contentType = QStringLiteral("application/ogg");
    } else if (m_codec == QStringLiteral("opus")) {
        contentType = QStringLiteral("audio/ogg; codecs=opus");
    } else if (m_codec == QStringLiteral("aac")) {
        contentType = QStringLiteral("audio/aac");
    } else {
        contentType = QStringLiteral("audio/mpeg");
    }

    // Build Authorization header (Basic auth: "source:<password>" base64-encoded)
    QByteArray credentials = QStringLiteral("source:%1").arg(m_password).toUtf8();
    QByteArray authBase64 = credentials.toBase64();

    // Ensure mount point starts with /
    QString mount = m_mountPoint;
    if (!mount.startsWith(QLatin1Char('/'))) {
        mount.prepend(QLatin1Char('/'));
    }

    // Assemble the HTTP request
    QByteArray request;
    request.append(QStringLiteral("SOURCE %1 HTTP/1.0\r\n").arg(mount).toUtf8());
    request.append(QStringLiteral("Authorization: Basic %1\r\n").arg(QString::fromLatin1(authBase64)).toUtf8());
    request.append(QStringLiteral("Content-Type: %1\r\n").arg(contentType).toUtf8());
    request.append(QStringLiteral("User-Agent: Mcaster1DAWCast/1.0\r\n").toUtf8());

    // ICY metadata headers
    if (!m_streamName.isEmpty()) {
        request.append(QStringLiteral("ice-name: %1\r\n").arg(m_streamName).toUtf8());
    }
    if (!m_streamDescription.isEmpty()) {
        request.append(QStringLiteral("ice-description: %1\r\n").arg(m_streamDescription).toUtf8());
    }
    if (!m_streamGenre.isEmpty()) {
        request.append(QStringLiteral("ice-genre: %1\r\n").arg(m_streamGenre).toUtf8());
    }
    if (!m_streamUrl.isEmpty()) {
        request.append(QStringLiteral("ice-url: %1\r\n").arg(m_streamUrl).toUtf8());
    }
    request.append(QStringLiteral("ice-public: %1\r\n").arg(m_isPublic ? 1 : 0).toUtf8());
    request.append(QStringLiteral("ice-bitrate: %1\r\n").arg(m_bitrate).toUtf8());
    request.append("\r\n"); // End of headers

    m_socket->write(request);
    m_socket->flush();
}

void StreamEncoder::onSocketDisconnected()
{
    qDebug() << "StreamEncoder: Socket disconnected";
    m_authenticated = false;

    if (m_streaming) {
        // Attempt reconnect
        qDebug() << "StreamEncoder: Will attempt reconnect in 5 seconds";
        m_reconnectTimer->start();
    }
}

void StreamEncoder::onSocketReadyRead()
{
    QByteArray response = m_socket->readAll();
    QString responseStr = QString::fromUtf8(response);

    qDebug() << "StreamEncoder: Server response:" << responseStr.trimmed();

    // Check for successful Icecast response
    // Icecast 2.x returns "HTTP/1.0 200 OK" on success
    if (responseStr.contains(QStringLiteral("200 OK")) ||
        responseStr.contains(QStringLiteral("200"))) {
        m_authenticated = true;
        qDebug() << "StreamEncoder: Authenticated, ready to stream";
        emit connected();
    } else if (responseStr.contains(QStringLiteral("401")) ||
               responseStr.contains(QStringLiteral("403"))) {
        qWarning() << "StreamEncoder: Authentication failed";
        m_streaming = false;
        m_socket->disconnectFromHost();
        emit error(QStringLiteral("Authentication failed: %1").arg(responseStr.trimmed()));
    } else if (responseStr.contains(QStringLiteral("mountpoint in use")) ||
               responseStr.contains(QStringLiteral("403"))) {
        qWarning() << "StreamEncoder: Mount point already in use";
        m_streaming = false;
        m_socket->disconnectFromHost();
        emit error(QStringLiteral("Mount point in use"));
    } else {
        qWarning() << "StreamEncoder: Unexpected server response:" << responseStr.trimmed();
        emit error(QStringLiteral("Unexpected server response: %1").arg(responseStr.trimmed()));
    }
}

void StreamEncoder::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    QString errMsg = m_socket->errorString();
    qWarning() << "StreamEncoder: Socket error:" << errMsg;

    if (m_streaming) {
        emit error(QStringLiteral("Connection error: %1").arg(errMsg));
        // Attempt reconnect
        m_reconnectTimer->start();
    }
}

} // namespace dawcast
