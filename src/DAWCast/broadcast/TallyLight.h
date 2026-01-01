// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

namespace dawcast {

class TallyLight : public QObject
{
    Q_OBJECT

public:
    explicit TallyLight(QObject *parent = nullptr);
    ~TallyLight() override;

    void setOnAir(bool onAir);
    void setRecording(bool recording);

    bool isOnAir() const;
    bool isRecording() const;

Q_SIGNALS:
    void stateChanged();

private:
    bool m_onAir{false};
    bool m_recording{false};
};

} // namespace dawcast
