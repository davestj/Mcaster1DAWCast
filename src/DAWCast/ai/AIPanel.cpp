// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AIPanel.h"
#include "AIEngine.h"
#include "TranscriptionEngine.h"
#include "AIAssistant.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QLineEdit>
#include <QSlider>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollArea>
#include <QSplitter>

namespace dawcast::ai {

AIPanel::AIPanel(QWidget* parent)
    : QWidget(parent)
    , m_engine(AIEngine::instance())
    , m_transcriber(new TranscriptionEngine(this))
    , m_assistant(new AIAssistant(this))
{
    setupUi();
    setupConnections();

    // Initial state: check if AI is available
    setUiEnabled(false);
    m_engine->checkConnection();
}

AIPanel::~AIPanel() = default;

// ── UI Construction ────────────────────────────────────────────────────────

void AIPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ── Top bar: backend selector + status ─────────────────────────────
    auto* topBar = new QHBoxLayout;

    m_backendCombo = new QComboBox;
    m_backendCombo->addItem(tr("Ollama (Local)"),   static_cast<int>(AIEngine::Ollama));
    m_backendCombo->addItem(tr("OpenAI"),           static_cast<int>(AIEngine::OpenAI));
    m_backendCombo->addItem(tr("Claude"),           static_cast<int>(AIEngine::Claude));
    m_backendCombo->addItem(tr("Disabled"),         static_cast<int>(AIEngine::Disabled));
    m_backendCombo->setCurrentIndex(3); // Disabled by default
    topBar->addWidget(m_backendCombo);

    m_statusDot = new QLabel;
    m_statusDot->setFixedSize(12, 12);
    updateStatusIndicator(false, tr("Not connected"));
    topBar->addWidget(m_statusDot);

    m_statusLabel = new QLabel(tr("Not connected"));
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    topBar->addWidget(m_statusLabel);

    mainLayout->addLayout(topBar);

    // ── Scrollable content area ────────────────────────────────────────
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContent = new QWidget;
    auto* scrollLayout  = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(6);

    scrollLayout->addWidget(createTranscriptionSection());
    scrollLayout->addWidget(createAssistantSection());
    scrollLayout->addWidget(createSettingsSection());

    // ── Output view ────────────────────────────────────────────────────
    auto* outputGroup = new QGroupBox(tr("Output"));
    auto* outputLayout = new QVBoxLayout(outputGroup);
    m_outputView = new QTextEdit;
    m_outputView->setReadOnly(true);
    m_outputView->setPlaceholderText(
        tr("AI results will appear here. Configure a backend above to get started."));
    m_outputView->setMinimumHeight(100);
    outputLayout->addWidget(m_outputView);
    scrollLayout->addWidget(outputGroup);

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}

QGroupBox* AIPanel::createTranscriptionSection()
{
    auto* group  = new QGroupBox(tr("Transcription (Whisper)"));
    auto* layout = new QVBoxLayout(group);

    // Model + language row
    auto* row1 = new QHBoxLayout;
    row1->addWidget(new QLabel(tr("Model:")));
    m_whisperModelCombo = new QComboBox;
    m_whisperModelCombo->addItems({
        tr("tiny"),  tr("base"), tr("small"),
        tr("medium"), tr("large")
    });
    m_whisperModelCombo->setCurrentIndex(1); // base
    row1->addWidget(m_whisperModelCombo);

    row1->addWidget(new QLabel(tr("Language:")));
    m_languageCombo = new QComboBox;
    m_languageCombo->addItems({
        QStringLiteral("en"), QStringLiteral("es"), QStringLiteral("fr"),
        QStringLiteral("de"), QStringLiteral("ja"), QStringLiteral("zh"),
        QStringLiteral("ko"), QStringLiteral("pt"), QStringLiteral("it"),
        QStringLiteral("auto")
    });
    row1->addWidget(m_languageCombo);
    layout->addLayout(row1);

    // Transcribe button + progress
    auto* row2 = new QHBoxLayout;
    m_transcribeBtn = new QPushButton(tr("Transcribe Audio..."));
    m_transcribeBtn->setToolTip(tr("Select an audio file to transcribe with Whisper"));
    row2->addWidget(m_transcribeBtn);

    m_transcribeProgress = new QProgressBar;
    m_transcribeProgress->setRange(0, 100);
    m_transcribeProgress->setValue(0);
    m_transcribeProgress->setVisible(false);
    row2->addWidget(m_transcribeProgress);
    layout->addLayout(row2);

    // Transcript output
    m_transcriptView = new QTextEdit;
    m_transcriptView->setReadOnly(true);
    m_transcriptView->setPlaceholderText(
        tr("Transcript will appear here after transcription completes."));
    m_transcriptView->setMaximumHeight(150);
    layout->addWidget(m_transcriptView);

    // Whisper availability hint
    if (!m_transcriber->isAvailable()) {
        auto* hint = new QLabel(
            tr("<i>whisper-cpp not found. Install via <code>brew install whisper-cpp</code> "
               "or set the path in Settings below.</i>"));
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: #999;"));
        layout->addWidget(hint);
    }

    // Populate available models
    QStringList models = TranscriptionEngine::availableModels();
    if (!models.isEmpty()) {
        m_whisperModelCombo->clear();
        for (const QString& m : models) {
            QFileInfo fi(m);
            m_whisperModelCombo->addItem(fi.fileName(), m);
        }
    }

    return group;
}

QGroupBox* AIPanel::createAssistantSection()
{
    auto* group  = new QGroupBox(tr("AI Assistant"));
    auto* layout = new QVBoxLayout(group);

    m_showNotesBtn = new QPushButton(tr("Generate Show Notes"));
    m_showNotesBtn->setToolTip(tr("Generate podcast show notes from the transcript above"));
    layout->addWidget(m_showNotesBtn);

    m_chaptersBtn = new QPushButton(tr("Suggest Chapter Markers"));
    m_chaptersBtn->setToolTip(tr("Auto-detect topic boundaries in the transcript"));
    layout->addWidget(m_chaptersBtn);

    m_titleBtn = new QPushButton(tr("Generate Episode Title"));
    m_titleBtn->setToolTip(tr("Suggest compelling episode titles from the transcript"));
    layout->addWidget(m_titleBtn);

    m_autoTagBtn = new QPushButton(tr("Auto-Tag Files..."));
    m_autoTagBtn->setToolTip(tr("Select audio files and auto-generate genre/mood/keyword tags"));
    layout->addWidget(m_autoTagBtn);

    m_mixAssistBtn = new QPushButton(tr("Mixing Assistant..."));
    m_mixAssistBtn->setToolTip(tr("Get EQ, compression, and mix suggestions for your project"));
    layout->addWidget(m_mixAssistBtn);

    return group;
}

QGroupBox* AIPanel::createSettingsSection()
{
    m_settingsGroup = new QGroupBox(tr("Settings"));
    m_settingsGroup->setCheckable(true);
    m_settingsGroup->setChecked(false); // collapsed by default

    auto* layout = new QFormLayout(m_settingsGroup);

    m_endpointEdit = new QLineEdit;
    m_endpointEdit->setPlaceholderText(QStringLiteral("http://localhost:11434"));
    layout->addRow(tr("Endpoint:"), m_endpointEdit);

    m_apiKeyEdit = new QLineEdit;
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(tr("For cloud APIs only"));
    layout->addRow(tr("API Key:"), m_apiKeyEdit);

    m_modelEdit = new QLineEdit;
    m_modelEdit->setPlaceholderText(QStringLiteral("llama3.1:8b"));
    layout->addRow(tr("Model:"), m_modelEdit);

    // Memory limit slider (for local model guidance)
    auto* memRow = new QHBoxLayout;
    m_memorySlider = new QSlider(Qt::Horizontal);
    m_memorySlider->setRange(2, 48);
    m_memorySlider->setValue(8);
    m_memorySlider->setTickInterval(4);
    m_memorySlider->setTickPosition(QSlider::TicksBelow);
    memRow->addWidget(m_memorySlider);

    m_memoryLabel = new QLabel(QStringLiteral("8 GB"));
    m_memoryLabel->setMinimumWidth(50);
    memRow->addWidget(m_memoryLabel);
    layout->addRow(tr("Memory Limit:"), memRow);

    // Model size recommendations
    auto* recommend = new QLabel(
        tr("<small>2-4 GB: tiny models (1-3B) | 8 GB: 7-8B models (recommended) | "
           "16 GB: 13B models | 32-48 GB: 30-70B models</small>"));
    recommend->setWordWrap(true);
    recommend->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addRow(recommend);

    m_applyBtn = new QPushButton(tr("Apply && Connect"));
    layout->addRow(m_applyBtn);

    return m_settingsGroup;
}

// ── Signal/Slot Wiring ─────────────────────────────────────────────────────

void AIPanel::setupConnections()
{
    // Backend selector
    connect(m_backendCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AIPanel::onBackendChanged);

    // Engine status
    connect(m_engine, &AIEngine::connectionStatus,
            this, &AIPanel::onConnectionStatus);

    // Transcription
    connect(m_transcribeBtn, &QPushButton::clicked,
            this, &AIPanel::onTranscribeClicked);
    connect(m_transcriber, &TranscriptionEngine::progress,
            this, &AIPanel::onTranscriptionProgress);
    connect(m_transcriber, &TranscriptionEngine::transcriptionComplete,
            this, &AIPanel::onTranscriptionComplete);
    connect(m_transcriber, &TranscriptionEngine::error,
            this, &AIPanel::onTranscriptionError);

    // Assistant buttons
    connect(m_showNotesBtn, &QPushButton::clicked,
            this, &AIPanel::onGenerateShowNotes);
    connect(m_chaptersBtn, &QPushButton::clicked,
            this, &AIPanel::onSuggestChapters);
    connect(m_titleBtn, &QPushButton::clicked,
            this, &AIPanel::onGenerateTitle);
    connect(m_autoTagBtn, &QPushButton::clicked,
            this, &AIPanel::onAutoTagFiles);
    connect(m_mixAssistBtn, &QPushButton::clicked,
            this, &AIPanel::onMixingAssistant);

    // Assistant results
    connect(m_assistant, &AIAssistant::showNotesReady,
            this, &AIPanel::onShowNotesReady);
    connect(m_assistant, &AIAssistant::titleSuggestionReady,
            this, &AIPanel::onTitleReady);
    connect(m_assistant, &AIAssistant::chaptersReady,
            this, &AIPanel::onChaptersReady);
    connect(m_assistant, &AIAssistant::tagsReady,
            this, &AIPanel::onTagsReady);
    connect(m_assistant, &AIAssistant::mixSuggestionsReady,
            this, &AIPanel::onMixSuggestionsReady);
    connect(m_assistant, &AIAssistant::error,
            this, &AIPanel::onAssistantError);

    // Settings
    connect(m_applyBtn, &QPushButton::clicked,
            this, &AIPanel::onApplySettings);
    connect(m_memorySlider, &QSlider::valueChanged, this, [this](int val) {
        m_memoryLabel->setText(QStringLiteral("%1 GB").arg(val));
        QString hint;
        if (val <= 4)       hint = tr(" (tiny 1-3B models)");
        else if (val <= 10) hint = tr(" (7-8B models, recommended)");
        else if (val <= 20) hint = tr(" (13B models)");
        else if (val <= 40) hint = tr(" (30-70B models)");
        else                hint = tr(" (70B models -- high memory)");
        m_memoryLabel->setText(QStringLiteral("%1 GB%2").arg(val).arg(hint));
    });
}

// ── Slot Implementations ───────────────────────────────────────────────────

void AIPanel::onBackendChanged(int index)
{
    auto backend = static_cast<AIEngine::Backend>(
        m_backendCombo->itemData(index).toInt());
    m_engine->setBackend(backend);

    if (backend == AIEngine::Disabled) {
        setUiEnabled(false);
        updateStatusIndicator(false, tr("AI disabled"));
    } else {
        // Auto-fill endpoint for convenience
        if (backend == AIEngine::Ollama)
            m_endpointEdit->setPlaceholderText(QStringLiteral("http://localhost:11434"));
        else if (backend == AIEngine::OpenAI)
            m_endpointEdit->setPlaceholderText(QStringLiteral("https://api.openai.com"));
        else if (backend == AIEngine::Claude)
            m_endpointEdit->setPlaceholderText(QStringLiteral("https://api.anthropic.com"));

        updateStatusIndicator(false, tr("Connecting..."));
        m_engine->checkConnection();
    }
}

void AIPanel::onConnectionStatus(bool available, const QString& modelName)
{
    setUiEnabled(available);
    if (available) {
        updateStatusIndicator(true, tr("Connected: %1").arg(modelName));
        m_modelEdit->setPlaceholderText(modelName);
    } else {
        updateStatusIndicator(false,
            modelName.isEmpty() ? tr("Connection failed")
                                : tr("Failed: %1").arg(modelName));
    }
}

void AIPanel::onApplySettings()
{
    QString endpoint = m_endpointEdit->text().trimmed();
    if (!endpoint.isEmpty())
        m_engine->setEndpoint(endpoint);

    QString apiKey = m_apiKeyEdit->text().trimmed();
    if (!apiKey.isEmpty())
        m_engine->setApiKey(apiKey);

    QString model = m_modelEdit->text().trimmed();
    if (!model.isEmpty())
        m_engine->setModel(model);

    m_engine->setMemoryLimitGB(m_memorySlider->value());

    updateStatusIndicator(false, tr("Connecting..."));
    m_engine->checkConnection();
}

// ── Transcription ──────────────────────────────────────────────────────────

void AIPanel::onTranscribeClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Audio File"), QString(),
        tr("Audio Files (*.wav *.mp3 *.flac *.ogg *.m4a *.aac *.opus);;All Files (*)"));
    if (path.isEmpty())
        return;

    // Configure model from UI selection
    TranscriptionEngine::Config cfg = m_transcriber->config();
    cfg.language = m_languageCombo->currentText();

    // Use model path from combo data, or construct from model name
    QVariant modelData = m_whisperModelCombo->currentData();
    if (modelData.isValid() && !modelData.toString().isEmpty()) {
        cfg.modelPath = modelData.toString();
    } else {
        // Construct path from model size name
        QString modelName = m_whisperModelCombo->currentText();
        cfg.modelPath = TranscriptionEngine::defaultModelDir()
                        + QStringLiteral("/ggml-%1.bin").arg(modelName);
    }
    m_transcriber->setConfig(cfg);

    m_transcribeProgress->setVisible(true);
    m_transcribeProgress->setValue(0);
    m_transcribeBtn->setEnabled(false);
    m_transcribeBtn->setText(tr("Transcribing..."));
    m_transcriptView->clear();

    m_transcriber->transcribe(path);
}

void AIPanel::onTranscriptionProgress(int percent)
{
    m_transcribeProgress->setValue(percent);
}

void AIPanel::onTranscriptionComplete(const QString& path, const QString& text)
{
    m_transcriptView->setPlainText(text);
    m_transcribeProgress->setValue(100);
    m_transcribeBtn->setEnabled(true);
    m_transcribeBtn->setText(tr("Transcribe Audio..."));
    m_outputView->append(tr("-- Transcription saved to: %1\n").arg(path));
}

void AIPanel::onTranscriptionError(const QString& msg)
{
    m_transcribeBtn->setEnabled(true);
    m_transcribeBtn->setText(tr("Transcribe Audio..."));
    m_transcribeProgress->setVisible(false);
    m_outputView->append(tr("Transcription error: %1\n").arg(msg));
}

// ── Assistant Feature Slots ────────────────────────────────────────────────

void AIPanel::onGenerateShowNotes()
{
    QString transcript = m_transcriptView->toPlainText();
    if (transcript.trimmed().isEmpty()) {
        m_outputView->append(tr("No transcript available. Transcribe an audio "
                                "file first, or paste a transcript above.\n"));
        return;
    }
    m_outputView->append(tr("-- Generating show notes...\n"));
    m_assistant->generateShowNotes(transcript);
}

void AIPanel::onSuggestChapters()
{
    QString transcript = m_transcriptView->toPlainText();
    if (transcript.trimmed().isEmpty()) {
        m_outputView->append(tr("No transcript available.\n"));
        return;
    }
    m_outputView->append(tr("-- Detecting chapter markers...\n"));
    m_assistant->generateChapterMarkers(transcript);
}

void AIPanel::onGenerateTitle()
{
    QString transcript = m_transcriptView->toPlainText();
    if (transcript.trimmed().isEmpty()) {
        m_outputView->append(tr("No transcript available.\n"));
        return;
    }
    m_outputView->append(tr("-- Generating episode title suggestions...\n"));
    m_assistant->generateEpisodeTitle(transcript);
}

void AIPanel::onAutoTagFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Select Audio Files to Tag"), QString(),
        tr("Audio Files (*.wav *.mp3 *.flac *.ogg *.m4a *.aac *.opus);;All Files (*)"));
    if (files.isEmpty())
        return;
    m_outputView->append(tr("-- Auto-tagging %1 file(s)...\n").arg(files.size()));
    m_assistant->autoTagFromContent(files);
}

void AIPanel::onMixingAssistant()
{
    // Simple inline prompt — in production, this could be a dialog
    m_outputView->append(tr("-- Generating mix suggestions...\n"));
    m_assistant->suggestMixSettings(QStringLiteral("podcast/voiceover"), 4);
}

// ── Result Display Slots ───────────────────────────────────────────────────

void AIPanel::onShowNotesReady(const QString& markdown)
{
    m_outputView->append(tr("== Show Notes ==\n%1\n").arg(markdown));
}

void AIPanel::onTitleReady(const QString& title)
{
    m_outputView->append(tr("== Episode Title Suggestions ==\n%1\n").arg(title));
}

void AIPanel::onChaptersReady(const QList<QPair<qint64, QString>>& chapters)
{
    QString text = tr("== Chapter Markers ==\n");
    for (const auto& [ms, title] : chapters) {
        int totalSec = static_cast<int>(ms / 1000);
        int h = totalSec / 3600;
        int m = (totalSec % 3600) / 60;
        int s = totalSec % 60;
        text += QStringLiteral("  %1:%2:%3  %4\n")
                    .arg(h, 2, 10, QLatin1Char('0'))
                    .arg(m, 2, 10, QLatin1Char('0'))
                    .arg(s, 2, 10, QLatin1Char('0'))
                    .arg(title);
    }
    m_outputView->append(text);
}

void AIPanel::onTagsReady(const QStringList& tags)
{
    m_outputView->append(tr("== Tags ==\n  %1\n").arg(tags.join(QStringLiteral(", "))));
}

void AIPanel::onMixSuggestionsReady(const QString& suggestions)
{
    m_outputView->append(tr("== Mix Suggestions ==\n%1\n").arg(suggestions));
}

void AIPanel::onAssistantError(const QString& msg)
{
    m_outputView->append(tr("AI Error: %1\n").arg(msg));
}

// ── UI Helpers ─────────────────────────────────────────────────────────────

void AIPanel::setUiEnabled(bool aiAvailable)
{
    m_showNotesBtn->setEnabled(aiAvailable);
    m_chaptersBtn->setEnabled(aiAvailable);
    m_titleBtn->setEnabled(aiAvailable);
    m_autoTagBtn->setEnabled(aiAvailable);
    m_mixAssistBtn->setEnabled(aiAvailable);

    // Transcription depends on Whisper, not the LLM backend
    m_transcribeBtn->setEnabled(m_transcriber->isAvailable());
}

void AIPanel::updateStatusIndicator(bool available, const QString& text)
{
    QString color = available ? QStringLiteral("#4CAF50")     // green
                              : QStringLiteral("#F44336");    // red
    m_statusDot->setStyleSheet(
        QStringLiteral("background-color: %1; border-radius: 6px;").arg(color));
    if (m_statusLabel)
        m_statusLabel->setText(text);
}

} // namespace dawcast::ai
