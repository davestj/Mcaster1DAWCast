// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QStringList>

namespace dawcast::platform {

QStringList audioInputDevices();
QStringList audioOutputDevices();
QStringList videoInputDevices();

} // namespace dawcast::platform
