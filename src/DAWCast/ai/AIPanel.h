// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class QPushButton;
class QProgressBar;
class QTextEdit;
class QLineEdit;
class QSlider;
class QGroupBox;
class QVBoxLayout;

namespace dawcast::ai {

class AIEngine;
class TranscriptionEngine;
class AIAssistant;

/// Dock-widget panel exposing AI features: transcription, show-notes
/// generation, chapter detection, mix assistant, and backend settings.
class AIPanel : public QWidget {
    Q_OBJECT

public:
    explicit AIPanel(QWidget* parent = nullptr);
    ~AIPanel() override;

private slots:
    // Backend
    void onBackendChanged(int index);
    void onConnectionStatus(bool available, const QString& modelName);

    // Transcription
    void onTranscribeClicked();
    void onTranscriptionProgress(int percent);
    void onTranscriptionComplete(const QString& path, const QString& text);
    void onTranscriptionError(const QString& msg);

    // Assistant features
    void onGenerateShowNotes();
    void onSuggestChapters();
    void onAutoTagFiles();
    void onMixingAssistant();
    void onGenerateTitle();

    // Results
    void onShowNotesReady(const QString& markdown);
    void onTitleReady(const QString& title);
    void onChaptersReady(const QList<QPair<qint64, QString>>& chapters);
    void onTagsReady(const QStringList& tags);
    void onMixSuggestionsReady(const QString& suggestions);
    void onAssistantError(const QString& msg);

    // Settings
    void onApplySettings();

private:
    void setupUi();
    void setupConnections();
    QGroupBox* createTranscriptionSection();
    QGroupBox* createAssistantSection();
    QGroupBox* createSettingsSection();
    void setUiEnabled(bool aiAvailable);
    void updateStatusIndicator(bool available, const QString& text);

    // Engine references (singletons / owned)
    AIEngine*            m_engine      = nullptr;
    TranscriptionEngine* m_transcriber = nullptr;
    AIAssistant*         m_assistant   = nullptr;

    // Top bar
    QComboBox*    m_backendCombo    = nullptr;
    QLabel*       m_statusDot       = nullptr;
    QLabel*       m_statusLabel     = nullptr;

    // Transcription section
    QPushButton*  m_transcribeBtn   = nullptr;
    QComboBox*    m_whisperModelCombo = nullptr;
    QComboBox*    m_languageCombo   = nullptr;
    QProgressBar* m_transcribeProgress = nullptr;
    QTextEdit*    m_transcriptView  = nullptr;

    // Assistant section
    QPushButton*  m_showNotesBtn    = nullptr;
    QPushButton*  m_chaptersBtn     = nullptr;
    QPushButton*  m_titleBtn        = nullptr;
    QPushButton*  m_autoTagBtn      = nullptr;
    QPushButton*  m_mixAssistBtn    = nullptr;

    // Settings section
    QLineEdit*    m_endpointEdit    = nullptr;
    QLineEdit*    m_apiKeyEdit      = nullptr;
    QLineEdit*    m_modelEdit       = nullptr;
    QSlider*      m_memorySlider    = nullptr;
    QLabel*       m_memoryLabel     = nullptr;
    QPushButton*  m_applyBtn        = nullptr;
    QGroupBox*    m_settingsGroup   = nullptr;

    // Output area
    QTextEdit*    m_outputView      = nullptr;
};

} // namespace dawcast::ai
