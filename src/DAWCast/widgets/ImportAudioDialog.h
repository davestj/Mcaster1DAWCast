// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QStringList>

class QTableWidget;
class QCheckBox;
class QComboBox;
class QRadioButton;
class QGroupBox;
class QPushButton;

namespace dawcast::widgets {

/// Audio file import dialog — shows file info and import options before
/// bringing audio files into the project (similar to Pro Tools Import Audio).
class ImportAudioDialog : public QDialog {
    Q_OBJECT

public:
    /// Import placement options for audio files on the timeline.
    enum Placement {
        AtPlayhead,
        AtStart,
        AtEnd
    };
    Q_ENUM(Placement)

    /// Aggregated import options returned after the user accepts the dialog.
    struct ImportOptions {
        bool copyToProjectFolder = true;
        bool convertSampleRate   = false;
        int  targetSampleRate    = 48000;
        bool convertBitDepth     = false;
        int  targetBitDepth      = 32;
        bool createNewTrack      = true;   ///< false = add to selected track
        Placement placement      = AtPlayhead;
    };

    explicit ImportAudioDialog(const QStringList& filePaths,
                               QWidget* parent = nullptr);
    ~ImportAudioDialog() override;

    /// Returns the user's chosen import options (valid after accept()).
    [[nodiscard]] ImportOptions options() const;

    /// Returns the file paths (possibly reordered or filtered by the user).
    [[nodiscard]] QStringList filePaths() const;

private:
    void buildFileInfoTable();
    void buildOptionsSection();
    QString formatDuration(double seconds) const;
    QString formatFileSize(qint64 bytes) const;

    QStringList m_filePaths;

    // File info table
    QTableWidget* m_fileTable = nullptr;

    // Options widgets
    QCheckBox*    m_copyToProjectCheck    = nullptr;
    QCheckBox*    m_convertSampleRateCheck = nullptr;
    QComboBox*    m_targetSampleRateCombo = nullptr;
    QCheckBox*    m_convertBitDepthCheck  = nullptr;
    QComboBox*    m_targetBitDepthCombo   = nullptr;
    QRadioButton* m_newTrackRadio         = nullptr;
    QRadioButton* m_selectedTrackRadio    = nullptr;
    QRadioButton* m_placeAtPlayhead       = nullptr;
    QRadioButton* m_placeAtStart          = nullptr;
    QRadioButton* m_placeAtEnd            = nullptr;
};

} // namespace dawcast::widgets
