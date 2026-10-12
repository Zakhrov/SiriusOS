#pragma once

#include "NoirWM.h"

// Window decorations: a frame window around each normal client with a title
// bar (icon and title on the left; minimize, maximize/restore and close on
// the right) and resize borders. Follows the VDD: black and silver, sharp
// corners, controls invert under the pointer.

#define FRAME_BORDER 4      // left, right and bottom
#define FRAME_TITLE 28      // top, including the 1 px outline
#define FRAME_BUTTON_WIDTH 36

#define FRAME_EDGE_LEFT   1
#define FRAME_EDGE_RIGHT  2
#define FRAME_EDGE_TOP    4
#define FRAME_EDGE_BOTTOM 8

void frame_init(NoirWM* wm);
void frame_shutdown(NoirWM* wm);

// Create the frame at the client's stored geometry and reparent the client in.
void frame_create(NoirWM* wm, NoirClient* client);
// Destroy the frame, putting the client back on the root at the same place.
void frame_destroy(NoirWM* wm, NoirClient* client);
void frame_draw(NoirWM* wm, NoirClient* client);

void frame_read_title(NoirWM* wm, NoirClient* client);
void frame_read_icon(NoirWM* wm, NoirClient* client);
void frame_read_size_hints(NoirWM* wm, NoirClient* client);

// Push the client's stored geometry to the X server (frame and client
// window) and tell the client where it ended up.
void frame_apply_geometry(NoirWM* wm, NoirClient* client);
void frame_constrain_size(const NoirClient* client, int* width, int* height);

void frame_handle_button_press(NoirWM* wm, NoirClient* client, XButtonEvent* event);
void frame_handle_button_release(NoirWM* wm, XButtonEvent* event);
void frame_handle_motion(NoirWM* wm, XMotionEvent* event);
void frame_handle_leave(NoirWM* wm, NoirClient* client);
