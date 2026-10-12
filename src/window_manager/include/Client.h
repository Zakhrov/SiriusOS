#pragma once

#include <X11/Xlib.h>

#include "NoirWM.h"

NoirClient* client_find(NoirWM* wm, Window window);
NoirClient* client_find_frame(NoirWM* wm, Window frame);
// The window that represents the client on the root: its frame, or itself.
Window client_toplevel(const NoirClient* client);
NoirClient* client_manage(NoirWM* wm, Window window);
void client_unmanage(NoirWM* wm, Window window);

// Map (if minimized), raise and focus a client, and publish _NET_ACTIVE_WINDOW.
void client_activate(NoirWM* wm, NoirClient* client);
void client_minimize(NoirWM* wm, NoirClient* client);
void client_close(NoirWM* wm, NoirClient* client);
void client_set_maximized(NoirWM* wm, NoirClient* client, int maximized);
// Usable screen area: x, y, width, height (the screen minus dock struts).
void wm_get_workarea(NoirWM* wm, long area[4]);
// Activate the next window after the active one (Alt+Tab).
void wm_cycle_focus(NoirWM* wm);
// Re-read window type and state, then restack; used when a client changes them.
void client_update_type(NoirWM* wm, NoirClient* client);
void client_update_strut(NoirWM* wm, NoirClient* client);

// Enforce layers: desktop < below < normal < above (docks, popups, notifications).
void wm_restack(NoirWM* wm);
void wm_update_client_lists(NoirWM* wm);
void wm_update_workarea(NoirWM* wm);
void wm_set_active(NoirWM* wm, Window window);
// Give focus to the topmost visible normal client, or to nobody.
void wm_focus_fallback(NoirWM* wm);

void handle_map_request(NoirWM* wm, XMapRequestEvent* event);
void handle_configure_request(NoirWM* wm, XConfigureRequestEvent* event);
