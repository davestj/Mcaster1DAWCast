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
// TagToFilenameDialog — rename audio files based on their tag metadata
//
// Substitutes %field% placeholders with actual tag values, sanitizes the
// resulting filename, and optionally creates directory structure.
// ---------------------------------------------------------------------------

class TagToFilenameDialog : public QDialog {
    Q_OBJECT

public:
    /// Construct with a list of file paths whose tags have already been read.
    /// The tagMap provides the current (possibly edited) tags for each file.
    explicit TagToFilenameDialog(const QStringList& filePaths,
                                 const QMap<QString, AudioTags>& tagMap,
                                 QWidget* parent = nullptr);
    ~TagToFilenameDialog() override;

    /// Returns old-path -> new-path mapping for files to rename.
    QMap<QString, QString> renames() const;

private slots:
    void updatePreview();

private:
    void setupUi();
    QString buildNewPath(const QString& oldPath, const AudioTags& tags,
                         const QString& pattern) const;
    static QString sanitizeFilename(const QString& name);

    QStringList              m_filePaths;
    QMap<QString, AudioTags> m_tagMap;
    QMap<QString, QString>   m_renames;

    // UI controls
    QLineEdit*    m_patternEdit    = nullptr;
    QComboBox*    m_presetCombo    = nullptr;
    QCheckBox*    m_createDirs     = nullptr;
    QCheckBox*    m_handleDupes    = nullptr;
    QTableWidget* m_previewTable   = nullptr;
};

} // namespace dawcast::widgets
