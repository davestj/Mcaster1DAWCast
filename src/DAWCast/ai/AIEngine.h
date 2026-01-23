// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace dawcast::ai {

/// Central AI integration manager.
///
/// Provides async text generation and chat completion via local (Ollama) or
/// remote (OpenAI, Claude) LLM backends.  All heavy I/O is non-blocking;
/// results arrive through signals.
class AIEngine : public QObject {
    Q_OBJECT

public:
    static AIEngine* instance();

    // ── Backend configuration ──────────────────────────────────────────
    enum Backend { Ollama, OpenAI, Claude, Disabled };
    Q_ENUM(Backend)

    void setBackend(Backend backend);
    Backend backend() const;

    void setEndpoint(const QString& url);   // e.g. "http://localhost:11434"
    QString endpoint() const;

    void setApiKey(const QString& key);     // For cloud APIs
    void setModel(const QString& model);    // e.g. "llama3.1:8b", "gpt-4o-mini"
    QString model() const;

    /// Maximum VRAM/memory budget in GB (for local model selection guidance).
    void setMemoryLimitGB(int gb);
    int  memoryLimitGB() const;

    // ── Availability ───────────────────────────────────────────────────
    bool isAvailable() const;

    /// Probe the configured backend asynchronously.
    /// Emits connectionStatus() on completion.
    void checkConnection();

    // ── Text generation (async) ────────────────────────────────────────

    /// Single-shot text generation.  Result arrives via textGenerated().
    void generateText(const QString& prompt, const QString& requestId = {});

    /// Chat completion with message history.
    /// Each entry in @p messages alternates "user" / "assistant" roles,
    /// starting with "user".  Result via textGenerated().
    void chatCompletion(const QStringList& messages,
                        const QString& requestId = {});

signals:
    void connectionStatus(bool available, const QString& modelName);
    void textGenerated(const QString& requestId, const QString& result);
    void generationError(const QString& requestId, const QString& error);

private:
    explicit AIEngine(QObject* parent = nullptr);
    ~AIEngine() override;

    // HTTP helpers
    void postOllama(const QByteArray& payload, const QString& requestId);
    void postOpenAI(const QByteArray& payload, const QString& requestId);
    void postClaude(const QByteArray& payload, const QString& requestId);

    void handleOllamaReply(QNetworkReply* reply, const QString& requestId);
    void handleOpenAIReply(QNetworkReply* reply, const QString& requestId);
    void handleClaudeReply(QNetworkReply* reply, const QString& requestId);

    QNetworkAccessManager* m_nam = nullptr;

    Backend  m_backend       = Disabled;
    QString  m_endpoint;
    QString  m_apiKey;
    QString  m_model;
    int      m_memoryLimitGB = 8;
    bool     m_available     = false;

    static AIEngine* s_instance;
};

} // namespace dawcast::ai
