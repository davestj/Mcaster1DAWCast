// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TagToFilenameDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>
#include <QDialogButtonBox>

namespace dawcast::widgets {

TagToFilenameDialog::TagToFilenameDialog(const QStringList& filePaths,
                                         const QMap<QString, AudioTags>& tagMap,
                                         QWidget* parent)
    : QDialog(parent)
    , m_filePaths(filePaths)
    , m_tagMap(tagMap)
{
    setWindowTitle(tr("Filename from Tags"));
    setMinimumSize(700, 500);
    setupUi();
    updatePreview();
}

TagToFilenameDialog::~TagToFilenameDialog() = default;

QMap<QString, QString> TagToFilenameDialog::renames() const
{
    return m_renames;
}

void TagToFilenameDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── Pattern section ────────────────────────────────────────────────
    auto* patternGroup = new QGroupBox(tr("Rename Pattern"), this);
    auto* patternLayout = new QFormLayout(patternGroup);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem(tr("Custom"), QString());
    m_presetCombo->addItem(tr("%artist% - %title%"),
                           QStringLiteral("%artist% - %title%"));
    m_presetCombo->addItem(tr("%tracknumber%. %title%"),
                           QStringLiteral("%tracknumber%. %title%"));
    m_presetCombo->addItem(tr("%tracknumber% - %artist% - %title%"),
                           QStringLiteral("%tracknumber% - %artist% - %title%"));
    m_presetCombo->addItem(tr("%artist%/%album%/%tracknumber% - %title%"),
                           QStringLiteral("%artist%/%album%/%tracknumber% - %title%"));
    m_presetCombo->addItem(tr("%artist% - %album% - %tracknumber%. %title%"),
                           QStringLiteral("%artist% - %album% - %tracknumber%. %title%"));
    patternLayout->addRow(tr("Preset:"), m_presetCombo);

    m_patternEdit = new QLineEdit(QStringLiteral("%artist% - %title%"), this);
    m_patternEdit->setPlaceholderText(tr("e.g. %artist% - %title%"));
    patternLayout->addRow(tr("Pattern:"), m_patternEdit);

    auto* hintLabel = new QLabel(
        tr("Placeholders: %artist%, %title%, %album%, %tracknumber%, %year%, "
           "%genre%, %albumartist%, %discnumber%, %composer%"),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    patternLayout->addRow(hintLabel);

    mainLayout->addWidget(patternGroup);

    // ── Options ────────────────────────────────────────────────────────
    auto* optionsLayout = new QHBoxLayout();

    m_createDirs = new QCheckBox(tr("Create directories if needed"), this);
    m_createDirs->setChecked(true);
    optionsLayout->addWidget(m_createDirs);

    m_handleDupes = new QCheckBox(tr("Append number for duplicates"), this);
    m_handleDupes->setChecked(true);
    optionsLayout->addWidget(m_handleDupes);

    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

    // ── Preview table ──────────────────────────────────────────────────
    auto* previewLabel = new QLabel(tr("Preview:"), this);
    mainLayout->addWidget(previewLabel);

    m_previewTable = new QTableWidget(this);
    m_previewTable->setColumnCount(2);
    m_previewTable->setHorizontalHeaderLabels({
        tr("Current Filename"), tr("New Filename")
    });
    m_previewTable->horizontalHeader()->setStretchLastSection(true);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setAlternatingRowColors(true);
    m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_previewTable->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_previewTable, 1);

    // ── Buttons ────────────────────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    // Connections
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        QString pattern = m_presetCombo->itemData(idx).toString();
        if (!pattern.isEmpty())
            m_patternEdit->setText(pattern);
    });
    connect(m_patternEdit, &QLineEdit::textChanged, this, &TagToFilenameDialog::updatePreview);
    connect(m_createDirs, &QCheckBox::toggled, this, &TagToFilenameDialog::updatePreview);
    connect(m_handleDupes, &QCheckBox::toggled, this, &TagToFilenameDialog::updatePreview);

    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, [this]() {
        // Build final renames map
        m_renames.clear();
        const QString pattern = m_patternEdit->text().trimmed();
        QSet<QString> usedPaths;

        for (const QString& fp : m_filePaths) {
            auto it = m_tagMap.constFind(fp);
            if (it == m_tagMap.constEnd()) continue;

            QString newPath = buildNewPath(fp, it.value(), pattern);
            if (newPath.isEmpty() || newPath == fp) continue;

            // Handle duplicates
            if (m_handleDupes->isChecked() && usedPaths.contains(newPath)) {
                QFileInfo nfi(newPath);
                int counter = 2;
                do {
                    newPath = nfi.absolutePath() + QLatin1Char('/') +
                              nfi.completeBaseName() +
                              QStringLiteral(" (%1).").arg(counter++) +
                              nfi.suffix();
                } while (usedPaths.contains(newPath));
            }
            usedPaths.insert(newPath);
            m_renames.insert(fp, newPath);
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void TagToFilenameDialog::updatePreview()
{
    const QString pattern = m_patternEdit->text().trimmed();
    m_previewTable->setRowCount(m_filePaths.size());

    for (int i = 0; i < m_filePaths.size(); ++i) {
        const QString& fp = m_filePaths.at(i);
        QFileInfo fi(fp);

        QString newName;
        auto it = m_tagMap.constFind(fp);
        if (it != m_tagMap.constEnd()) {
            QString newPath = buildNewPath(fp, it.value(), pattern);
            if (!newPath.isEmpty())
                newName = QFileInfo(newPath).fileName();
        }

        m_previewTable->setItem(i, 0, new QTableWidgetItem(fi.fileName()));
        m_previewTable->setItem(i, 1, new QTableWidgetItem(
            newName.isEmpty() ? tr("(no change)") : newName));
    }
    m_previewTable->resizeColumnsToContents();
}

QString TagToFilenameDialog::buildNewPath(const QString& oldPath,
                                           const AudioTags& tags,
                                           const QString& pattern) const
{
    if (pattern.isEmpty()) return {};

    QFileInfo fi(oldPath);
    QString result = pattern;

    // Substitute placeholders with tag values
    result.replace(QStringLiteral("%artist%"),
                   tags.artist.isEmpty() ? QStringLiteral("Unknown Artist") : tags.artist);
    result.replace(QStringLiteral("%title%"),
                   tags.title.isEmpty() ? fi.completeBaseName() : tags.title);
    result.replace(QStringLiteral("%album%"),
                   tags.album.isEmpty() ? QStringLiteral("Unknown Album") : tags.album);
    result.replace(QStringLiteral("%albumartist%"),
                   tags.albumArtist.isEmpty() ? tags.artist : tags.albumArtist);
    result.replace(QStringLiteral("%genre%"),
                   tags.genre.isEmpty() ? QStringLiteral("Unknown") : tags.genre);
    result.replace(QStringLiteral("%composer%"),
                   tags.composer.isEmpty() ? QString() : tags.composer);
    result.replace(QStringLiteral("%year%"),
                   tags.year > 0 ? QString::number(tags.year) : QStringLiteral("0000"));

    // Track/disc numbers with zero-padding
    result.replace(QStringLiteral("%tracknumber%"),
                   tags.trackNumber > 0
                       ? QStringLiteral("%1").arg(tags.trackNumber, 2, 10, QLatin1Char('0'))
                       : QStringLiteral("00"));
    result.replace(QStringLiteral("%discnumber%"),
                   tags.discNumber > 0 ? QString::number(tags.discNumber) : QStringLiteral("1"));

    // Handle directory separators in the pattern
    bool hasDir = result.contains(QLatin1Char('/'));
    if (hasDir) {
        // Split into dir part and filename part
        int lastSlash = result.lastIndexOf(QLatin1Char('/'));
        QString dirPart = result.left(lastSlash);
        QString namePart = result.mid(lastSlash + 1);

        // Sanitize each directory component
        QStringList dirComponents = dirPart.split(QLatin1Char('/'));
        for (QString& comp : dirComponents)
            comp = sanitizeFilename(comp);

        namePart = sanitizeFilename(namePart);

        // Build the new path relative to the original file's parent
        QString baseDir = fi.absolutePath();
        if (m_createDirs->isChecked()) {
            QString newDir = baseDir + QLatin1Char('/') + dirComponents.join(QLatin1Char('/'));
            return newDir + QLatin1Char('/') + namePart + QLatin1Char('.') + fi.suffix();
        }
        // Without directory creation, flatten to just the filename
        return baseDir + QLatin1Char('/') + namePart + QLatin1Char('.') + fi.suffix();
    }

    // Simple case: no directory structure
    result = sanitizeFilename(result);
    return fi.absolutePath() + QLatin1Char('/') + result + QLatin1Char('.') + fi.suffix();
}

QString TagToFilenameDialog::sanitizeFilename(const QString& name)
{
    QString safe = name;
    // Replace characters illegal in filenames across platforms
    static const QRegularExpression illegalChars(QStringLiteral("[\\\\:*?\"<>|]"));
    safe.replace(illegalChars, QStringLiteral("_"));
    // Trim leading/trailing whitespace and dots
    safe = safe.trimmed();
    while (safe.endsWith(QLatin1Char('.')))
        safe.chop(1);
    while (safe.startsWith(QLatin1Char('.')))
        safe.remove(0, 1);
    // Collapse multiple spaces
    static const QRegularExpression multiSpace(QStringLiteral("\\s{2,}"));
    safe.replace(multiSpace, QStringLiteral(" "));
    return safe;
}

} // namespace dawcast::widgets
