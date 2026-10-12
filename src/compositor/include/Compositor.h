#pragma once
#include <X11/Xlib.h>

// X11 compositing manager. Redirects every top-level window off screen,
// mirrors their contents into Vulkan textures as they are damaged, and
// paints the stack onto the composite overlay window.
//
// The compositor shares the window manager's Display connection. The caller
// must have SubstructureNotifyMask selected on the root window and must pass
// every X event to compositor_handle_event().
typedef struct NoirCompositor NoirCompositor;

// Returns NULL (after printing why) when the X server lacks an extension or
// Vulkan cannot present; the caller can carry on without compositing.
NoirCompositor* compositor_create(Display* display, Window root);
void compositor_destroy(NoirCompositor* compositor);

void compositor_handle_event(NoirCompositor* compositor, XEvent* event);
// Whether anything changed since the last compositor_draw().
int compositor_needs_frame(const NoirCompositor* compositor);
// Milliseconds until the next frame may be drawn (0: now). Frames are paced
// to the display rate so a busy client cannot make the compositor spin.
int compositor_frame_delay(const NoirCompositor* compositor);
void compositor_draw(NoirCompositor* compositor);

// Give a top-level window (a child of the root) a drop shadow.
void compositor_set_shadow(NoirCompositor* compositor, Window toplevel, int enabled);
const char* compositor_device_name(const NoirCompositor* compositor);
