// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QStringList>
#include <QMap>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>

#include "../codec/TagTransfer.h"

namespace dawcast::widgets {

// ---------------------------------------------------------------------------
// FilenameToTagDialog — parse metadata from filenames using user patterns
//
// Supports patterns like:
//   %artist% - %title%
//   %artist%/%album%/%tracknumber% - %title%
//   %artist%_%album%_%title%
//
// Placeholders: %artist%, %title%, %album%, %tracknumber%, %year%, %genre%,
//               %albumartist%, %discnumber%, %composer%, %dummy% (discard)
// ---------------------------------------------------------------------------

class FilenameToTagDialog : public QDialog {
    Q_OBJECT

public:
    explicit FilenameToTagDialog(const QStringList& filePaths,
                                 QWidget* parent = nullptr);
    ~FilenameToTagDialog() override;

    /// Returns the parsed tags for each file path (only files that matched).
    QMap<QString, AudioTags> results() const;

    enum CaseConversion {
        KeepOriginal = 0,
        TitleCase,
        UpperCase,
        LowerCase,
        SentenceCase
    };

private slots:
    void updatePreview();

private:
    void setupUi();
    AudioTags parseFilename(const QString& filePath, const QString& pattern) const;
    QString   applyCase(const QString& input, CaseConversion mode) const;
    static QStringList tokenizePattern(const QString& pattern);

    QStringList              m_filePaths;
    QMap<QString, AudioTags> m_results;

    // UI controls
    QLineEdit*    m_patternEdit     = nullptr;
    QComboBox*    m_presetCombo     = nullptr;
    QComboBox*    m_caseCombo       = nullptr;
    QCheckBox*    m_underscoreToSpace = nullptr;
    QTableWidget* m_previewTable    = nullptr;
};

} // namespace dawcast::widgets
