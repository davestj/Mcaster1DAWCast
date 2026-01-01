// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UndoManager.h"
#include <QUndoStack>

namespace dawcast {

UndoManager::UndoManager(QObject* parent)
    : QObject(parent)
    , m_stack(new QUndoStack(this))
{
    connect(m_stack, &QUndoStack::undoTextChanged, this, &UndoManager::undoTextChanged);
    connect(m_stack, &QUndoStack::redoTextChanged, this, &UndoManager::redoTextChanged);
}

UndoManager::~UndoManager() = default;

void UndoManager::push(QUndoCommand* command)
{
    if (command) {
        m_stack->push(command);
    }
}

void UndoManager::undo()
{
    if (m_stack->canUndo()) {
        m_stack->undo();
    }
}

void UndoManager::redo()
{
    if (m_stack->canRedo()) {
        m_stack->redo();
    }
}

bool UndoManager::canUndo() const
{
    return m_stack->canUndo();
}

bool UndoManager::canRedo() const
{
    return m_stack->canRedo();
}

} // namespace dawcast
