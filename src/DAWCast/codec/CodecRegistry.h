// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

namespace dawcast {

struct CodecInfo {
    QString id;              // e.g., "mp3", "aac", "opus"
    QString name;            // e.g., "MP3 (LAME)"
    bool canEncode{false};
    bool canDecode{false};
    QStringList extensions;  // e.g., {"mp3"}
};

class CodecRegistry : public QObject
{
    Q_OBJECT

public:
    static CodecRegistry &instance();

    void registerCodec(const CodecInfo &info);
    QList<CodecInfo> availableCodecs() const;
    CodecInfo codecById(const QString &id) const;
    bool canEncode(const QString &id) const;
    bool canDecode(const QString &id) const;

private:
    explicit CodecRegistry(QObject *parent = nullptr);
    ~CodecRegistry() override;
    CodecRegistry(const CodecRegistry &) = delete;
    CodecRegistry &operator=(const CodecRegistry &) = delete;

    void populateDefaults();

    QMap<QString, CodecInfo> m_codecs;
};

} // namespace dawcast
