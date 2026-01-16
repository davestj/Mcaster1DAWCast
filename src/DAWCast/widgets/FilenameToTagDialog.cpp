// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FilenameToTagDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QPushButton>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDialogButtonBox>

namespace dawcast::widgets {

// Known pattern placeholders and which AudioTags field they map to
static const QStringList kPlaceholders = {
    QStringLiteral("%artist%"),
    QStringLiteral("%title%"),
    QStringLiteral("%album%"),
    QStringLiteral("%tracknumber%"),
    QStringLiteral("%year%"),
    QStringLiteral("%genre%"),
    QStringLiteral("%albumartist%"),
    QStringLiteral("%discnumber%"),
    QStringLiteral("%composer%"),
    QStringLiteral("%dummy%")
};

FilenameToTagDialog::FilenameToTagDialog(const QStringList& filePaths,
                                         QWidget* parent)
    : QDialog(parent)
    , m_filePaths(filePaths)
{
    setWindowTitle(tr("Auto-Tag from Filename"));
    setMinimumSize(700, 500);
    setupUi();
    updatePreview();
}

FilenameToTagDialog::~FilenameToTagDialog() = default;

QMap<QString, AudioTags> FilenameToTagDialog::results() const
{
    return m_results;
}

void FilenameToTagDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── Pattern section ────────────────────────────────────────────────
    auto* patternGroup = new QGroupBox(tr("Filename Pattern"), this);
    auto* patternLayout = new QFormLayout(patternGroup);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem(tr("Custom"), QString());
    m_presetCombo->addItem(tr("%artist% - %title%"),
                           QStringLiteral("%artist% - %title%"));
    m_presetCombo->addItem(tr("%tracknumber%. %artist% - %title%"),
                           QStringLiteral("%tracknumber%. %artist% - %title%"));
    m_presetCombo->addItem(tr("%tracknumber% - %title%"),
                           QStringLiteral("%tracknumber% - %title%"));
    m_presetCombo->addItem(tr("%artist% - %album% - %tracknumber%. %title%"),
                           QStringLiteral("%artist% - %album% - %tracknumber%. %title%"));
    m_presetCombo->addItem(tr("%artist%/%album%/%tracknumber% - %title%"),
                           QStringLiteral("%artist%/%album%/%tracknumber% - %title%"));
    m_presetCombo->addItem(tr("%artist%_%album%_%title%"),
                           QStringLiteral("%artist%_%album%_%title%"));
    patternLayout->addRow(tr("Preset:"), m_presetCombo);

    m_patternEdit = new QLineEdit(QStringLiteral("%artist% - %title%"), this);
    m_patternEdit->setPlaceholderText(tr("e.g. %artist% - %title%"));
    patternLayout->addRow(tr("Pattern:"), m_patternEdit);

    auto* hintLabel = new QLabel(
        tr("Placeholders: %artist%, %title%, %album%, %tracknumber%, %year%, "
           "%genre%, %albumartist%, %discnumber%, %composer%, %dummy%"),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    patternLayout->addRow(hintLabel);

    mainLayout->addWidget(patternGroup);

    // ── Options section ────────────────────────────────────────────────
    auto* optionsLayout = new QHBoxLayout();

    m_caseCombo = new QComboBox(this);
    m_caseCombo->addItem(tr("Keep Original"),  KeepOriginal);
    m_caseCombo->addItem(tr("Title Case"),     TitleCase);
    m_caseCombo->addItem(tr("UPPERCASE"),      UpperCase);
    m_caseCombo->addItem(tr("lowercase"),      LowerCase);
    m_caseCombo->addItem(tr("Sentence case"),  SentenceCase);
    optionsLayout->addWidget(new QLabel(tr("Case:")), 0);
    optionsLayout->addWidget(m_caseCombo, 0);

    m_underscoreToSpace = new QCheckBox(tr("Convert underscores to spaces"), this);
    m_underscoreToSpace->setChecked(true);
    optionsLayout->addWidget(m_underscoreToSpace);

    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

    // ── Preview table ──────────────────────────────────────────────────
    auto* previewLabel = new QLabel(tr("Preview (first 20 files):"), this);
    mainLayout->addWidget(previewLabel);

    m_previewTable = new QTableWidget(this);
    m_previewTable->setColumnCount(7);
    m_previewTable->setHorizontalHeaderLabels({
        tr("Filename"), tr("Artist"), tr("Title"), tr("Album"),
        tr("Track #"), tr("Year"), tr("Genre")
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
    connect(m_patternEdit, &QLineEdit::textChanged, this, &FilenameToTagDialog::updatePreview);
    connect(m_caseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilenameToTagDialog::updatePreview);
    connect(m_underscoreToSpace, &QCheckBox::toggled, this, &FilenameToTagDialog::updatePreview);

    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, [this]() {
        // Build final results for all files
        m_results.clear();
        const QString pattern = m_patternEdit->text().trimmed();
        for (const QString& fp : m_filePaths) {
            AudioTags tags = parseFilename(fp, pattern);
            if (!tags.isEmpty())
                m_results.insert(fp, tags);
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FilenameToTagDialog::updatePreview()
{
    const QString pattern = m_patternEdit->text().trimmed();
    const int previewCount = qMin(m_filePaths.size(), 20);

    m_previewTable->setRowCount(previewCount);

    for (int i = 0; i < previewCount; ++i) {
        const QString& fp = m_filePaths.at(i);
        AudioTags tags = parseFilename(fp, pattern);

        m_previewTable->setItem(i, 0, new QTableWidgetItem(QFileInfo(fp).fileName()));
        m_previewTable->setItem(i, 1, new QTableWidgetItem(tags.artist));
        m_previewTable->setItem(i, 2, new QTableWidgetItem(tags.title));
        m_previewTable->setItem(i, 3, new QTableWidgetItem(tags.album));
        m_previewTable->setItem(i, 4, new QTableWidgetItem(
            tags.trackNumber > 0 ? QString::number(tags.trackNumber) : QString()));
        m_previewTable->setItem(i, 5, new QTableWidgetItem(
            tags.year > 0 ? QString::number(tags.year) : QString()));
        m_previewTable->setItem(i, 6, new QTableWidgetItem(tags.genre));
    }
    m_previewTable->resizeColumnsToContents();
}

AudioTags FilenameToTagDialog::parseFilename(const QString& filePath,
                                              const QString& pattern) const
{
    AudioTags tags;
    if (pattern.isEmpty()) return tags;

    // Determine whether the pattern uses directory separators
    const bool usesDir = pattern.contains(QLatin1Char('/'));
    const CaseConversion caseMode =
        static_cast<CaseConversion>(m_caseCombo->currentData().toInt());
    const bool underscoreReplace = m_underscoreToSpace->isChecked();

    // Build the input string: either just filename (no ext) or relative path components
    QFileInfo fi(filePath);
    QString input;
    if (usesDir) {
        // Use the last N directory components that match the number of '/' in the pattern
        const int slashCount = pattern.count(QLatin1Char('/'));
        QStringList parts;
        QString dir = fi.absolutePath();
        for (int s = 0; s < slashCount && !dir.isEmpty(); ++s) {
            QFileInfo d(dir);
            parts.prepend(d.fileName());
            dir = d.absolutePath();
        }
        parts.append(fi.completeBaseName());
        input = parts.join(QLatin1Char('/'));
    } else {
        input = fi.completeBaseName();
    }

    // Tokenize the pattern into a list of literals and placeholders
    QStringList tokens = tokenizePattern(pattern);

    // Build a regex from the pattern tokens
    // Each placeholder becomes a named capture group; literals become escaped literals
    QString regexStr;
    QStringList captureNames;
    for (const QString& tok : tokens) {
        if (tok.startsWith(QLatin1Char('%')) && tok.endsWith(QLatin1Char('%'))) {
            QString name = tok.mid(1, tok.size() - 2);
            // Use indexed group names to avoid duplicates
            QString groupName = name + QString::number(captureNames.size());
            captureNames.append(name);
            regexStr += QStringLiteral("(?<%1>.+?)").arg(groupName);
        } else {
            regexStr += QRegularExpression::escape(tok);
        }
    }
    // Make the last capture group greedy
    regexStr.replace(regexStr.lastIndexOf(QStringLiteral(".+?")),
                     3, QStringLiteral(".+"));

    QRegularExpression re(regexStr);
    QRegularExpressionMatch m = re.match(input);
    if (!m.hasMatch()) return tags;

    // Extract captured groups and assign to tags
    for (int i = 0; i < captureNames.size(); ++i) {
        const QString& name = captureNames.at(i);
        QString groupName = name + QString::number(i);
        QString val = m.captured(groupName).trimmed();

        // Optional underscore-to-space replacement
        if (underscoreReplace)
            val.replace(QLatin1Char('_'), QLatin1Char(' '));

        val = applyCase(val, caseMode);

        if (name == QLatin1String("artist"))           tags.artist = val;
        else if (name == QLatin1String("title"))       tags.title = val;
        else if (name == QLatin1String("album"))       tags.album = val;
        else if (name == QLatin1String("tracknumber")) tags.trackNumber = val.toInt();
        else if (name == QLatin1String("year"))        tags.year = val.toInt();
        else if (name == QLatin1String("genre"))       tags.genre = val;
        else if (name == QLatin1String("albumartist")) tags.albumArtist = val;
        else if (name == QLatin1String("discnumber"))  tags.discNumber = val.toInt();
        else if (name == QLatin1String("composer"))    tags.composer = val;
        // "dummy" is intentionally discarded
    }

    return tags;
}

QString FilenameToTagDialog::applyCase(const QString& input,
                                        CaseConversion mode) const
{
    switch (mode) {
    case UpperCase:
        return input.toUpper();
    case LowerCase:
        return input.toLower();
    case TitleCase: {
        QStringList words = input.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (QString& w : words) {
            if (w.size() > 0) {
                w[0] = w[0].toUpper();
                for (int i = 1; i < w.size(); ++i)
                    w[i] = w[i].toLower();
            }
        }
        return words.join(QLatin1Char(' '));
    }
    case SentenceCase: {
        if (input.isEmpty()) return input;
        QString result = input.toLower();
        result[0] = result[0].toUpper();
        return result;
    }
    default:
        return input;
    }
}

QStringList FilenameToTagDialog::tokenizePattern(const QString& pattern)
{
    // Split the pattern into alternating literal / placeholder tokens
    // e.g. "%artist% - %title%" -> ["%artist%", " - ", "%title%"]
    QStringList tokens;
    int pos = 0;
    static QRegularExpression placeholderRe(QStringLiteral("%[a-z]+%"));

    auto it = placeholderRe.globalMatch(pattern);
    while (it.hasNext()) {
        auto match = it.next();
        if (match.capturedStart() > pos) {
            tokens.append(pattern.mid(pos, match.capturedStart() - pos));
        }
        tokens.append(match.captured());
        pos = match.capturedEnd();
    }
    if (pos < pattern.size()) {
        tokens.append(pattern.mid(pos));
    }
    return tokens;
}

} // namespace dawcast::widgets
