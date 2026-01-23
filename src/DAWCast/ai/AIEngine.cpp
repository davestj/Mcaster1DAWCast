// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AIEngine.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUuid>

namespace dawcast::ai {

AIEngine* AIEngine::s_instance = nullptr;

AIEngine* AIEngine::instance()
{
    if (!s_instance)
        s_instance = new AIEngine;
    return s_instance;
}

AIEngine::AIEngine(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

AIEngine::~AIEngine() = default;

// ── Configuration ──────────────────────────────────────────────────────────

void AIEngine::setBackend(Backend backend)
{
    m_backend = backend;
    m_available = false;

    // Set sensible default endpoints when the user switches backends
    if (backend == Ollama && m_endpoint.isEmpty())
        m_endpoint = QStringLiteral("http://localhost:11434");
    else if (backend == OpenAI && m_endpoint.isEmpty())
        m_endpoint = QStringLiteral("https://api.openai.com");
    else if (backend == Claude && m_endpoint.isEmpty())
        m_endpoint = QStringLiteral("https://api.anthropic.com");
}

AIEngine::Backend AIEngine::backend() const { return m_backend; }

void    AIEngine::setEndpoint(const QString& url) { m_endpoint = url; }
QString AIEngine::endpoint() const                { return m_endpoint; }

void AIEngine::setApiKey(const QString& key) { m_apiKey = key; }

void    AIEngine::setModel(const QString& model) { m_model = model; }
QString AIEngine::model() const                  { return m_model; }

void AIEngine::setMemoryLimitGB(int gb) { m_memoryLimitGB = qBound(2, gb, 64); }
int  AIEngine::memoryLimitGB() const    { return m_memoryLimitGB; }

bool AIEngine::isAvailable() const { return m_available && m_backend != Disabled; }

// ── Connection Check ───────────────────────────────────────────────────────

void AIEngine::checkConnection()
{
    if (m_backend == Disabled) {
        m_available = false;
        emit connectionStatus(false, QString());
        return;
    }

    if (m_backend == Ollama) {
        // GET /api/tags lists available models
        QUrl url(m_endpoint + QStringLiteral("/api/tags"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/json"));

        QNetworkReply* reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                m_available = false;
                emit connectionStatus(false, reply->errorString());
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray models = doc.object().value(QStringLiteral("models")).toArray();

            // If no model is configured, pick the first available one
            QString detected;
            if (!models.isEmpty()) {
                detected = models.first()
                               .toObject()
                               .value(QStringLiteral("name"))
                               .toString();
                if (m_model.isEmpty())
                    m_model = detected;
            }
            m_available = !models.isEmpty();
            emit connectionStatus(m_available, m_model);
        });
        return;
    }

    // For cloud APIs, do a lightweight test: send a trivial completion
    if (m_backend == OpenAI) {
        QJsonObject msg;
        msg[QStringLiteral("role")]    = QStringLiteral("user");
        msg[QStringLiteral("content")] = QStringLiteral("ping");

        QJsonObject body;
        body[QStringLiteral("model")]      = m_model.isEmpty()
            ? QStringLiteral("gpt-4o-mini") : m_model;
        body[QStringLiteral("messages")]   = QJsonArray{msg};
        body[QStringLiteral("max_tokens")] = 1;

        QUrl url(m_endpoint + QStringLiteral("/v1/chat/completions"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/json"));
        req.setRawHeader("Authorization",
                          QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());

        QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(
                                                     QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_available = (reply->error() == QNetworkReply::NoError);
            emit connectionStatus(m_available, m_model);
        });
        return;
    }

    if (m_backend == Claude) {
        QJsonObject msg;
        msg[QStringLiteral("role")]    = QStringLiteral("user");
        msg[QStringLiteral("content")] = QStringLiteral("ping");

        QJsonObject body;
        body[QStringLiteral("model")]      = m_model.isEmpty()
            ? QStringLiteral("claude-sonnet-4-20250514") : m_model;
        body[QStringLiteral("messages")]   = QJsonArray{msg};
        body[QStringLiteral("max_tokens")] = 1;

        QUrl url(m_endpoint + QStringLiteral("/v1/messages"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/json"));
        req.setRawHeader("x-api-key", m_apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");

        QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(
                                                     QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_available = (reply->error() == QNetworkReply::NoError);
            emit connectionStatus(m_available, m_model);
        });
        return;
    }
}

// ── Text Generation ────────────────────────────────────────────────────────

void AIEngine::generateText(const QString& prompt, const QString& requestId)
{
    const QString id = requestId.isEmpty()
                           ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                           : requestId;

    if (m_backend == Disabled || !m_available) {
        emit generationError(id, tr("AI backend is not available. "
                                    "Configure a backend in the AI panel settings."));
        return;
    }

    if (m_backend == Ollama) {
        QJsonObject body;
        body[QStringLiteral("model")]  = m_model;
        body[QStringLiteral("prompt")] = prompt;
        body[QStringLiteral("stream")] = false;
        postOllama(QJsonDocument(body).toJson(QJsonDocument::Compact), id);
        return;
    }

    // Wrap as a single-user-message chat for OpenAI / Claude
    chatCompletion({prompt}, id);
}

void AIEngine::chatCompletion(const QStringList& messages,
                              const QString& requestId)
{
    const QString id = requestId.isEmpty()
                           ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                           : requestId;

    if (m_backend == Disabled || !m_available) {
        emit generationError(id, tr("AI backend is not available."));
        return;
    }

    // Build role-alternating messages array
    QJsonArray msgArray;
    for (int i = 0; i < messages.size(); ++i) {
        QJsonObject m;
        m[QStringLiteral("role")]    = (i % 2 == 0) ? QStringLiteral("user")
                                                      : QStringLiteral("assistant");
        m[QStringLiteral("content")] = messages[i];
        msgArray.append(m);
    }

    if (m_backend == Ollama) {
        // Ollama /api/chat endpoint
        QJsonObject body;
        body[QStringLiteral("model")]    = m_model;
        body[QStringLiteral("messages")] = msgArray;
        body[QStringLiteral("stream")]   = false;

        QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

        QUrl url(m_endpoint + QStringLiteral("/api/chat"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/json"));
        req.setTransferTimeout(120000);

        QNetworkReply* reply = m_nam->post(req, payload);
        connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit generationError(id, reply->errorString());
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString text = doc.object()
                               .value(QStringLiteral("message")).toObject()
                               .value(QStringLiteral("content")).toString();
            emit textGenerated(id, text);
        });
        return;
    }

    if (m_backend == OpenAI) {
        QJsonObject body;
        body[QStringLiteral("model")]    = m_model.isEmpty()
            ? QStringLiteral("gpt-4o-mini") : m_model;
        body[QStringLiteral("messages")] = msgArray;
        postOpenAI(QJsonDocument(body).toJson(QJsonDocument::Compact), id);
        return;
    }

    if (m_backend == Claude) {
        QJsonObject body;
        body[QStringLiteral("model")]      = m_model.isEmpty()
            ? QStringLiteral("claude-sonnet-4-20250514") : m_model;
        body[QStringLiteral("messages")]   = msgArray;
        body[QStringLiteral("max_tokens")] = 4096;
        postClaude(QJsonDocument(body).toJson(QJsonDocument::Compact), id);
        return;
    }
}

// ── HTTP Helpers ───────────────────────────────────────────────────────────

void AIEngine::postOllama(const QByteArray& payload, const QString& requestId)
{
    QUrl url(m_endpoint + QStringLiteral("/api/generate"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                   QStringLiteral("application/json"));
    req.setTransferTimeout(120000);

    QNetworkReply* reply = m_nam->post(req, payload);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, requestId]() { handleOllamaReply(reply, requestId); });
}

void AIEngine::postOpenAI(const QByteArray& payload, const QString& requestId)
{
    QUrl url(m_endpoint + QStringLiteral("/v1/chat/completions"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                   QStringLiteral("application/json"));
    req.setRawHeader("Authorization",
                      QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());
    req.setTransferTimeout(120000);

    QNetworkReply* reply = m_nam->post(req, payload);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, requestId]() { handleOpenAIReply(reply, requestId); });
}

void AIEngine::postClaude(const QByteArray& payload, const QString& requestId)
{
    QUrl url(m_endpoint + QStringLiteral("/v1/messages"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                   QStringLiteral("application/json"));
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());
    req.setRawHeader("anthropic-version", "2023-06-01");
    req.setTransferTimeout(120000);

    QNetworkReply* reply = m_nam->post(req, payload);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, requestId]() { handleClaudeReply(reply, requestId); });
}

// ── Reply Handlers ─────────────────────────────────────────────────────────

void AIEngine::handleOllamaReply(QNetworkReply* reply, const QString& requestId)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit generationError(requestId, reply->errorString());
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QString text = doc.object().value(QStringLiteral("response")).toString();
    emit textGenerated(requestId, text);
}

void AIEngine::handleOpenAIReply(QNetworkReply* reply, const QString& requestId)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        QJsonDocument errDoc = QJsonDocument::fromJson(reply->readAll());
        QString msg = errDoc.object()
                          .value(QStringLiteral("error")).toObject()
                          .value(QStringLiteral("message")).toString();
        emit generationError(requestId, msg.isEmpty() ? reply->errorString() : msg);
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        emit generationError(requestId, tr("Empty response from OpenAI"));
        return;
    }
    QString text = choices.first().toObject()
                       .value(QStringLiteral("message")).toObject()
                       .value(QStringLiteral("content")).toString();
    emit textGenerated(requestId, text);
}

void AIEngine::handleClaudeReply(QNetworkReply* reply, const QString& requestId)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        QJsonDocument errDoc = QJsonDocument::fromJson(reply->readAll());
        QString msg = errDoc.object()
                          .value(QStringLiteral("error")).toObject()
                          .value(QStringLiteral("message")).toString();
        emit generationError(requestId, msg.isEmpty() ? reply->errorString() : msg);
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray content = doc.object().value(QStringLiteral("content")).toArray();
    if (content.isEmpty()) {
        emit generationError(requestId, tr("Empty response from Claude"));
        return;
    }
    // Concatenate all text blocks
    QString text;
    for (const QJsonValue& v : content) {
        if (v.toObject().value(QStringLiteral("type")).toString()
                == QStringLiteral("text")) {
            text += v.toObject().value(QStringLiteral("text")).toString();
        }
    }
    emit textGenerated(requestId, text);
}

} // namespace dawcast::ai
