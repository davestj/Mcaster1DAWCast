// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QColor>

class QGridLayout;

namespace dawcast::widgets {

/// A compact color picker dialog for changing track colors.
/// Presents a grid of preset colors matching the web version palette,
/// a "Custom..." button for QColorDialog, and a live preview.
class TrackColorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TrackColorDialog(const QColor& initialColor = QColor(0, 180, 180),
                              QWidget* parent = nullptr);
    ~TrackColorDialog() override;

    /// The color selected by the user (valid after accept()).
    [[nodiscard]] QColor selectedColor() const { return m_selectedColor; }

private slots:
    void onPresetClicked(const QColor& color);
    void onCustomColor();

private:
    void buildUI();
    void updatePreview();

    QColor       m_selectedColor;
    QWidget*     m_previewWidget = nullptr;
    QGridLayout* m_grid          = nullptr;
};

} // namespace dawcast::widgets
