// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Vst3HostMac.mm
// ──────────────
// macOS-only Obj-C bridge for Vst3Host. VST3 plugin editors on macOS
// want an NSView* passed to IPlugView::attached(kPlatformTypeNSView).
// QWidget::winId() on macOS returns an NSView* once WA_NativeWindow is
// set, so we cast it here and also expose a tiny resize helper that
// PlugFrame::resizeView() uses to bump the host container's NSView
// frame when a plugin requests a size change.

#import <Cocoa/Cocoa.h>
#include <QWidget>

namespace dawcast::plugins {

void* vst3ObtainNSViewForWidget(QWidget* widget)
{
    if (!widget) return nullptr;
    // WA_NativeWindow was set by the caller; winId() resolves to the NSView*.
    WId wid = widget->winId();
    return reinterpret_cast<void*>(wid);
}

void vst3ResizeNSView(void* nsView, int width, int height)
{
    if (!nsView) return;
    NSView* view = (__bridge NSView*)nsView;
    NSRect frame = [view frame];
    frame.size.width  = width;
    frame.size.height = height;
    [view setFrame:frame];
}

} // namespace dawcast::plugins
