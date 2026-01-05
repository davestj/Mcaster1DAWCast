// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ExportDialog.h"
#include "ProjectManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>

namespace dawcast::widgets {

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Export"));
    setMinimumSize(480, 520);
    auto* mainLayout = new QVBoxLayout(this);

    // --- Preset selector ---
    auto* presetRow = new QHBoxLayout;
    auto* presetLabel = new QLabel(tr("Preset:"), this);
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem(tr("Custom"));
    m_presetCombo->addItem(tr("Podcast (MP3 128kbps)"));
    m_presetCombo->addItem(tr("Podcast (AAC 192kbps)"));
    m_presetCombo->addItem(tr("Broadcast (FLAC 48kHz)"));
    m_presetCombo->addItem(tr("Video (H.264 1080p)"));
    m_presetCombo->addItem(tr("Video (VP9 720p)"));
    presetRow->addWidget(presetLabel);
    presetRow->addWidget(m_presetCombo, 1);
    mainLayout->addLayout(presetRow);

    connect(m_presetCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::applyPreset);

    // --- Tabs ---
    auto* tabs = new QTabWidget(this);

    // == Audio tab ==
    auto* audioTab = new QWidget(tabs);
    auto* audioLayout = new QFormLayout(audioTab);

    m_audioCodecCombo = new QComboBox(audioTab);
    m_audioCodecCombo->addItems({
        QStringLiteral("MP3"), QStringLiteral("AAC"), QStringLiteral("Opus"),
        QStringLiteral("Vorbis"), QStringLiteral("FLAC"), QStringLiteral("WAV")
    });
    audioLayout->addRow(tr("Codec:"), m_audioCodecCombo);

    m_bitrateSpin = new QSpinBox(audioTab);
    m_bitrateSpin->setRange(64, 320);
    m_bitrateSpin->setValue(256);
    m_bitrateSpin->setSuffix(QStringLiteral(" kbps"));
    m_bitrateSpin->setSingleStep(32);
    audioLayout->addRow(tr("Bitrate:"), m_bitrateSpin);

    m_sampleRateCombo = new QComboBox(audioTab);
    m_sampleRateCombo->addItems({
        QStringLiteral("44100"), QStringLiteral("48000"), QStringLiteral("96000")
    });
    m_sampleRateCombo->setCurrentIndex(1);
    audioLayout->addRow(tr("Sample Rate:"), m_sampleRateCombo);

    m_channelsCombo = new QComboBox(audioTab);
    m_channelsCombo->addItems({tr("Mono"), tr("Stereo")});
    m_channelsCombo->setCurrentIndex(1);
    audioLayout->addRow(tr("Channels:"), m_channelsCombo);

    tabs->addTab(audioTab, tr("Audio Only"));

    // Disable bitrate for lossless codecs
    connect(m_audioCodecCombo, &QComboBox::currentTextChanged, this, [this](const QString& codec) {
        bool lossless = (codec == QStringLiteral("FLAC") || codec == QStringLiteral("WAV"));
        m_bitrateSpin->setEnabled(!lossless);
    });

    // == Audio + Video tab ==
    auto* videoTab = new QWidget(tabs);
    auto* videoLayout = new QFormLayout(videoTab);

    m_videoCodecCombo = new QComboBox(videoTab);
    m_videoCodecCombo->addItems({
        QStringLiteral("H.264"), QStringLiteral("VP9"), QStringLiteral("Theora")
    });
    videoLayout->addRow(tr("Video Codec:"), m_videoCodecCombo);

    m_videoBitrateSpin = new QSpinBox(videoTab);
    m_videoBitrateSpin->setRange(500, 50000);
    m_videoBitrateSpin->setValue(5000);
    m_videoBitrateSpin->setSuffix(QStringLiteral(" kbps"));
    m_videoBitrateSpin->setSingleStep(500);
    videoLayout->addRow(tr("Video Bitrate:"), m_videoBitrateSpin);

    m_resolutionCombo = new QComboBox(videoTab);
    m_resolutionCombo->addItem(tr("720p (1280x720)"), QStringLiteral("1280x720"));
    m_resolutionCombo->addItem(tr("1080p (1920x1080)"), QStringLiteral("1920x1080"));
    m_resolutionCombo->addItem(tr("4K (3840x2160)"), QStringLiteral("3840x2160"));
    m_resolutionCombo->setCurrentIndex(1);
    videoLayout->addRow(tr("Resolution:"), m_resolutionCombo);

    m_fpsSpin = new QSpinBox(videoTab);
    m_fpsSpin->setRange(15, 120);
    m_fpsSpin->setValue(30);
    m_fpsSpin->setSuffix(QStringLiteral(" fps"));
    videoLayout->addRow(tr("Framerate:"), m_fpsSpin);

    m_containerCombo = new QComboBox(videoTab);
    m_containerCombo->addItems({
        QStringLiteral("MP4"), QStringLiteral("MKV"),
        QStringLiteral("WebM"), QStringLiteral("AVI")
    });
    videoLayout->addRow(tr("Container:"), m_containerCombo);

    // Audio settings repeated for video tab
    auto* videoAudioGroup = new QGroupBox(tr("Audio Settings"), videoTab);
    auto* vaLayout = new QFormLayout(videoAudioGroup);
    auto* vaCodec = new QLabel(tr("(Uses audio tab settings)"), videoAudioGroup);
    vaLayout->addRow(vaCodec);
    videoLayout->addRow(videoAudioGroup);

    tabs->addTab(videoTab, tr("Audio + Video"));

    mainLayout->addWidget(tabs);

    // --- Format selection (derived from active tab) ---
    m_formatCombo = nullptr; // We derive audioOnly from the active tab
    m_activeTab = tabs;

    // --- Output path ---
    auto* outputGroup = new QGroupBox(tr("Output"), this);
    auto* outputLayout = new QHBoxLayout(outputGroup);

    m_outputPathEdit = new QLineEdit(outputGroup);
    m_outputPathEdit->setPlaceholderText(tr("Select output file..."));
    outputLayout->addWidget(m_outputPathEdit, 1);

    auto* browseBtn = new QPushButton(tr("Browse..."), outputGroup);
    outputLayout->addWidget(browseBtn);

    connect(browseBtn, &QPushButton::clicked, this, [this] {
        QString filter;
        if (m_activeTab && m_activeTab->currentIndex() == 0) {
            filter = tr("Audio Files (*.mp3 *.aac *.opus *.ogg *.flac *.wav);;All Files (*)");
        } else {
            filter = tr("Video Files (*.mp4 *.mkv *.webm *.avi);;All Files (*)");
        }
        QString path = QFileDialog::getSaveFileName(this, tr("Export File"), QString(), filter);
        if (!path.isEmpty())
            m_outputPathEdit->setText(path);
    });

    mainLayout->addWidget(outputGroup);

    // --- OK / Cancel ---
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Store width/height spin pointers for exportSettings()
    m_widthSpin  = nullptr;
    m_heightSpin = nullptr;
}

ExportDialog::~ExportDialog() = default;

void ExportDialog::setProject(ProjectManager* project)
{
    m_project = project;

    if (m_project) {
        // Set sample rate from project
        int sr = m_project->sampleRate();
        for (int i = 0; i < m_sampleRateCombo->count(); ++i) {
            if (m_sampleRateCombo->itemText(i).toInt() == sr) {
                m_sampleRateCombo->setCurrentIndex(i);
                break;
            }
        }

        // Suggest output path based on project name
        if (m_outputPathEdit && m_outputPathEdit->text().isEmpty()) {
            m_outputPathEdit->setText(m_project->projectName() + QStringLiteral(".mp3"));
        }
    }
}

ExportSettings ExportDialog::exportSettings() const
{
    ExportSettings s;
    s.audioOnly    = (m_activeTab && m_activeTab->currentIndex() == 0);
    s.audioCodec   = m_audioCodecCombo->currentText().toLower();
    s.audioBitrate = m_bitrateSpin->value();
    s.sampleRate   = m_sampleRateCombo->currentText().toInt();
    s.videoCodec   = m_videoCodecCombo->currentText().toLower();
    s.videoFps     = m_fpsSpin->value();

    // Parse resolution from combo data
    QString res = m_resolutionCombo->currentData().toString();
    QStringList parts = res.split(QChar('x'));
    if (parts.size() == 2) {
        s.videoWidth  = parts[0].toInt();
        s.videoHeight = parts[1].toInt();
    } else {
        s.videoWidth  = 1920;
        s.videoHeight = 1080;
    }

    return s;
}

void ExportDialog::applyPreset(int index)
{
    switch (index) {
    case 1: // Podcast MP3 128
        m_audioCodecCombo->setCurrentText(QStringLiteral("MP3"));
        m_bitrateSpin->setValue(128);
        m_sampleRateCombo->setCurrentText(QStringLiteral("44100"));
        if (m_activeTab) m_activeTab->setCurrentIndex(0);
        break;
    case 2: // Podcast AAC 192
        m_audioCodecCombo->setCurrentText(QStringLiteral("AAC"));
        m_bitrateSpin->setValue(192);
        m_sampleRateCombo->setCurrentText(QStringLiteral("48000"));
        if (m_activeTab) m_activeTab->setCurrentIndex(0);
        break;
    case 3: // Broadcast FLAC
        m_audioCodecCombo->setCurrentText(QStringLiteral("FLAC"));
        m_sampleRateCombo->setCurrentText(QStringLiteral("48000"));
        if (m_activeTab) m_activeTab->setCurrentIndex(0);
        break;
    case 4: // Video H.264 1080p
        m_videoCodecCombo->setCurrentText(QStringLiteral("H.264"));
        m_resolutionCombo->setCurrentIndex(1);
        m_fpsSpin->setValue(30);
        m_videoBitrateSpin->setValue(5000);
        m_audioCodecCombo->setCurrentText(QStringLiteral("AAC"));
        m_bitrateSpin->setValue(192);
        if (m_activeTab) m_activeTab->setCurrentIndex(1);
        break;
    case 5: // Video VP9 720p
        m_videoCodecCombo->setCurrentText(QStringLiteral("VP9"));
        m_resolutionCombo->setCurrentIndex(0);
        m_fpsSpin->setValue(30);
        m_videoBitrateSpin->setValue(2500);
        m_audioCodecCombo->setCurrentText(QStringLiteral("Opus"));
        m_bitrateSpin->setValue(128);
        m_containerCombo->setCurrentText(QStringLiteral("WebM"));
        if (m_activeTab) m_activeTab->setCurrentIndex(1);
        break;
    default:
        break;
    }
}

} // namespace dawcast::widgets
