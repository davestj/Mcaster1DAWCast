// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>

class QUndoStack;
class QUndoCommand;

namespace dawcast {

class UndoManager : public QObject
{
    Q_OBJECT

public:
    explicit UndoManager(QObject* parent = nullptr);
    ~UndoManager() override;

    void push(QUndoCommand* command);
    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] QUndoStack* stack() const { return m_stack; }

signals:
    void undoTextChanged(const QString& text);
    void redoTextChanged(const QString& text);

private:
    QUndoStack* m_stack = nullptr;
};

} // namespace dawcast
