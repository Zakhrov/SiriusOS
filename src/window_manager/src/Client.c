#include "Client.h"
#include "Ewmh.h"
#include "Frame.h"

#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>

#define CASCADE_STEP 28
#define CASCADE_SLOTS 8

NoirClient* client_find(NoirWM* wm, Window window) {
    for (int i = 0; i < wm->clientCount; i++) {
        if (wm->clients[i].window == window) {
            return &wm->clients[i];
        }
    }
    return NULL;
}

NoirClient* client_find_frame(NoirWM* wm, Window frame) {
    for (int i = 0; i < wm->clientCount; i++) {
        if (wm->clients[i].frame == frame) {
            return &wm->clients[i];
        }
    }
    return NULL;
}

Window client_toplevel(const NoirClient* client) {
    return client->frame ? client->frame : client->window;
}

void wm_get_workarea(NoirWM* wm, long area[4]) {
    area[0] = 0; area[1] = 0;
    area[2] = wm->screenWidth; area[3] = wm->screenHeight;
    ewmh_get_cardinals(wm->display, wm->root, noirAtoms[NET_WORKAREA], area, 4);
}

// _MOTIF_WM_HINTS with the decorations field cleared: how toolkits ask for a
// frameless window (Qt.FramelessWindowHint, GTK undecorated).
static int motif_undecorated(NoirWM* wm, Window window) {
    long hints[5] = { 0 };
    Atom type;
    int format;
    unsigned long count = 0, remaining;
    unsigned char* data = NULL;
    int undecorated = 0;
    if (XGetWindowProperty(wm->display, window, noirAtoms[MOTIF_WM_HINTS], 0, 5, False, AnyPropertyType,
                           &type, &format, &count, &remaining, &data) == Success && data) {
        if (format == 32 && count >= 3) {
            memcpy(hints, data, sizeof(long) * (count < 5 ? count : 5));
            undecorated = (hints[0] & 2) && hints[2] == 0;   // MWM_HINTS_DECORATIONS
        }
        XFree(data);
    }
    return undecorated;
}

static void set_wm_state(NoirWM* wm, Window window, long state) {
    const long data[2] = { state, None };
    XChangeProperty(wm->display, window, noirAtoms[WM_STATE], noirAtoms[WM_STATE], 32,
                    PropModeReplace, (const unsigned char*)data, 2);
}

static void read_type(NoirWM* wm, NoirClient* client) {
    Display* d = wm->display;
    Atom* types;
    int typeCount = ewmh_get_atoms(d, client->window, noirAtoms[NET_WM_WINDOW_TYPE], &types);
    int isDesktop = 0, isDock = 0, isOverlay = 0, isUndecorated = 0;
    for (int i = 0; i < typeCount; i++) {
        Atom t = types[i];
        if (t == noirAtoms[NET_WM_WINDOW_TYPE_DESKTOP]) isDesktop = 1;
        else if (t == noirAtoms[NET_WM_WINDOW_TYPE_DOCK]) isDock = 1;
        else if (t == noirAtoms[NET_WM_WINDOW_TYPE_NOTIFICATION] ||
                 t == noirAtoms[NET_WM_WINDOW_TYPE_POPUP_MENU] ||
                 t == noirAtoms[NET_WM_WINDOW_TYPE_DROPDOWN_MENU] ||
                 t == noirAtoms[NET_WM_WINDOW_TYPE_TOOLTIP]) isOverlay = 1;
        else if (t == noirAtoms[NET_WM_WINDOW_TYPE_SPLASH]) isUndecorated = 1;
    }
    if (types) XFree(types);

    if (isDesktop) client->layer = LAYER_DESKTOP;
    else if (isDock || isOverlay) client->layer = LAYER_ABOVE;
    else if (ewmh_has_atom(d, client->window, noirAtoms[NET_WM_STATE], noirAtoms[NET_WM_STATE_ABOVE])) client->layer = LAYER_ABOVE;
    else if (ewmh_has_atom(d, client->window, noirAtoms[NET_WM_STATE], noirAtoms[NET_WM_STATE_BELOW])) client->layer = LAYER_BELOW;
    else client->layer = LAYER_NORMAL;

    client->decorated = !(isDesktop || isDock || isOverlay || isUndecorated) &&
                        !motif_undecorated(wm, client->window);
    client->isShellSurface = isDesktop || isDock || isOverlay;
    client->focusOnClick = !client->isShellSurface ||
        ewmh_has_atom(d, client->window, noirAtoms[NET_WM_WINDOW_TYPE], noirAtoms[NET_WM_WINDOW_TYPE_NOTIFICATION]);
    client->isDock = isDock;

    // WM_HINTS input and WM_TAKE_FOCUS describe separate focus mechanisms.
    client->acceptsFocus = 1;
    XWMHints* hints = XGetWMHints(d, client->window);
    if (hints) {
        if ((hints->flags & InputHint) && !hints->input) {
            client->acceptsFocus = 0;
        }
        XFree(hints);
    }
    client->takesFocus = ewmh_supports_protocol(d, client->window, noirAtoms[WM_TAKE_FOCUS]);
}

// Where a new decorated window goes: where it asked to be if it said so,
// otherwise cascading down from the top-left of the work area.
static void place_frame(NoirWM* wm, NoirClient* client, const XWindowAttributes* wa, int keepPosition) {
    long area[4];
    wm_get_workarea(wm, area);
    client->width = wa->width;
    client->height = wa->height;
    frame_constrain_size(client, &client->width, &client->height);
    const int maxWidth = (int)area[2] - 2 * FRAME_BORDER, maxHeight = (int)area[3] - FRAME_TITLE - FRAME_BORDER;
    if (client->width > maxWidth) client->width = maxWidth;
    if (client->height > maxHeight) client->height = maxHeight;
    const int fw = client->width + 2 * FRAME_BORDER, fh = client->height + FRAME_TITLE + FRAME_BORDER;

    XSizeHints hints;
    long supplied;
    const int positioned = XGetWMNormalHints(wm->display, client->window, &hints, &supplied) &&
                           (hints.flags & (USPosition | PPosition));
    if (keepPosition) {
        // Already on screen (NoirWM restarted): wrap the frame around it.
        client->x = wa->x - FRAME_BORDER;
        client->y = wa->y - FRAME_TITLE;
    } else if (positioned) {
        client->x = wa->x;
        client->y = wa->y;
    } else {
        const int offset = 32 + CASCADE_STEP * (wm->placementStep++ % CASCADE_SLOTS);
        client->x = (int)area[0] + 120 + offset;
        client->y = (int)area[1] + offset;
    }
    if (client->x + fw > area[0] + area[2]) client->x = (int)(area[0] + area[2]) - fw;
    if (client->y + fh > area[1] + area[3]) client->y = (int)(area[1] + area[3]) - fh;
    if (client->x < area[0]) client->x = (int)area[0];
    if (client->y < area[1]) client->y = (int)area[1];
}

// Give the client a frame, or take it away, to match client->decorated.
static void apply_decoration(NoirWM* wm, NoirClient* client, int keepPosition) {
    Display* d = wm->display;
    if (client->decorated && !client->frame) {
        XWindowAttributes wa;
        if (!XGetWindowAttributes(d, client->window, &wa)) {
            return;
        }
        frame_read_size_hints(wm, client);
        frame_read_title(wm, client);
        frame_read_icon(wm, client);
        place_frame(wm, client, &wa, keepPosition);
        frame_create(wm, client);
        frame_apply_geometry(wm, client);
        if (wa.map_state != IsUnmapped && !client->minimized) {
            XMapWindow(d, client->frame);
        }
    } else if (!client->decorated) {
        if (client->frame) {
            frame_destroy(wm, client);
        }
        XSetWindowBorderWidth(d, client->window, 0);
        const long extents[4] = { 0, 0, 0, 0 };
        ewmh_set_cardinals(d, client->window, noirAtoms[NET_FRAME_EXTENTS], extents, 4);
    }
}

void client_update_strut(NoirWM* wm, NoirClient* client) {
    long values[12] = { 0 };
    int n = ewmh_get_cardinals(wm->display, client->window, noirAtoms[NET_WM_STRUT_PARTIAL], values, 12);
    if (n < 4) {
        n = ewmh_get_cardinals(wm->display, client->window, noirAtoms[NET_WM_STRUT], values, 4);
    }
    for (int i = 0; i < 4; i++) {
        client->strut[i] = n >= 4 ? values[i] : 0;
    }
    wm_update_workarea(wm);
}

void client_update_type(NoirWM* wm, NoirClient* client) {
    read_type(wm, client);
    apply_decoration(wm, client, 1);
    wm_restack(wm);
}

// Keep new frameless normal windows inside the work area (clear of the taskbar).
static void place_client(NoirWM* wm, NoirClient* client) {
    if (client->layer != LAYER_NORMAL || client->frame) {
        return;
    }
    long area[4];
    wm_get_workarea(wm, area);

    XWindowAttributes wa;
    if (!XGetWindowAttributes(wm->display, client->window, &wa)) {
        return;
    }
    const int w = wa.width, h = wa.height;
    int x = wa.x, y = wa.y;
    if (x + w > area[0] + area[2]) x = (int)(area[0] + area[2]) - w;
    if (y + h > area[1] + area[3]) y = (int)(area[1] + area[3]) - h;
    if (x < area[0]) x = (int)area[0];
    if (y < area[1]) y = (int)area[1];
    if (x != wa.x || y != wa.y) {
        XMoveWindow(wm->display, client->window, x, y);
    }
}

NoirClient* client_manage(NoirWM* wm, Window window) {
    NoirClient* existing = client_find(wm, window);
    if (existing) {
        return existing;
    }
    if (wm->clientCount == wm->clientCapacity) {
        int capacity = wm->clientCapacity ? wm->clientCapacity * 2 : 16;
        NoirClient* grown = realloc(wm->clients, sizeof(NoirClient) * (size_t)capacity);
        if (!grown) {
            return NULL;
        }
        wm->clients = grown;
        wm->clientCapacity = capacity;
    }
    NoirClient* client = &wm->clients[wm->clientCount++];
    memset(client, 0, sizeof(*client));
    client->window = window;

    Display* d = wm->display;
    // Property changes drive title/state/strut updates; button grabs give
    // click-to-focus (the click is replayed to the client afterwards).
    XSelectInput(d, window, PropertyChangeMask);
    XGrabButton(d, AnyButton, AnyModifier, window, False, ButtonPressMask,
                GrabModeSync, GrabModeAsync, None, None);

    XWindowAttributes wa;
    const int alreadyMapped = XGetWindowAttributes(d, window, &wa) && wa.map_state != IsUnmapped;
    read_type(wm, client);
    apply_decoration(wm, client, alreadyMapped);
    if (client->frame && (ewmh_has_atom(d, window, noirAtoms[NET_WM_STATE], noirAtoms[NET_WM_STATE_MAXIMIZED_VERT]) ||
                          ewmh_has_atom(d, window, noirAtoms[NET_WM_STATE], noirAtoms[NET_WM_STATE_MAXIMIZED_HORZ]))) {
        client_set_maximized(wm, client, 1);
    }

    const long desktop = client->layer == LAYER_DESKTOP || client->layer == LAYER_ABOVE ? 0xFFFFFFFFL : 0;
    ewmh_set_cardinals(d, window, noirAtoms[NET_WM_DESKTOP], &desktop, 1);
    ewmh_change_state(d, window, NET_WM_STATE_REMOVE, noirAtoms[NET_WM_STATE_HIDDEN]);
    set_wm_state(wm, window, NormalState);

    client_update_strut(wm, client);
    place_client(wm, client);
    wm_update_client_lists(wm);
    return client;
}

void client_unmanage(NoirWM* wm, Window window) {
    for (int i = 0; i < wm->clientCount; i++) {
        if (wm->clients[i].window != window) {
            continue;
        }
        const int hadStrut = wm->clients[i].strut[0] || wm->clients[i].strut[1] ||
                             wm->clients[i].strut[2] || wm->clients[i].strut[3];
        frame_destroy(wm, &wm->clients[i]);
        if (wm->dragClient == window) {
            wm->dragMode = DRAG_NONE;
        }
        memmove(&wm->clients[i], &wm->clients[i + 1],
                sizeof(NoirClient) * (size_t)(wm->clientCount - i - 1));
        wm->clientCount--;
        if (hadStrut) {
            wm_update_workarea(wm);
        }
        wm_update_client_lists(wm);
        if (wm->activeWindow == window) {
            wm_focus_fallback(wm);
        }
        return;
    }
}

void wm_set_active(NoirWM* wm, Window window) {
    const Window previous = wm->activeWindow;
    wm->activeWindow = window;
    ewmh_set_windows(wm->display, wm->root, noirAtoms[NET_ACTIVE_WINDOW], &window, 1);
    if (previous == window) {
        return;
    }
    // Title bars show which window has the focus.
    NoirClient* old = client_find(wm, previous);
    if (old) {
        ewmh_change_state(wm->display, previous, NET_WM_STATE_REMOVE, noirAtoms[NET_WM_STATE_FOCUSED]);
        frame_draw(wm, old);
    }
    NoirClient* now = client_find(wm, window);
    if (now) {
        ewmh_change_state(wm->display, window, NET_WM_STATE_ADD, noirAtoms[NET_WM_STATE_FOCUSED]);
        frame_draw(wm, now);
    }
}

static void send_protocol(NoirWM* wm, Window window, Atom protocol) {
    XEvent event = { 0 };
    event.xclient.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = noirAtoms[WM_PROTOCOLS];
    event.xclient.format = 32;
    event.xclient.data.l[0] = (long)protocol;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(wm->display, window, False, NoEventMask, &event);
}

void client_activate(NoirWM* wm, NoirClient* client) {
    Display* d = wm->display;
    if (client->minimized) {
        client->minimized = 0;
        XMapWindow(d, client_toplevel(client));
        set_wm_state(wm, client->window, NormalState);
        ewmh_change_state(d, client->window, NET_WM_STATE_REMOVE, noirAtoms[NET_WM_STATE_HIDDEN]);
    }
    ewmh_change_state(d, client->window, NET_WM_STATE_REMOVE, noirAtoms[NET_WM_STATE_DEMANDS_ATTENTION]);
    if (client->layer != LAYER_DESKTOP) {
        XRaiseWindow(d, client_toplevel(client));
    }
    wm_restack(wm);

    if (client->acceptsFocus) {
        XSetInputFocus(d, client->window, RevertToPointerRoot, CurrentTime);
    }
    if (client->takesFocus) {
        send_protocol(wm, client->window, noirAtoms[WM_TAKE_FOCUS]);
    }
    wm_set_active(wm, client->window);
}

void client_minimize(NoirWM* wm, NoirClient* client) {
    if (client->minimized || client->layer == LAYER_DESKTOP) {
        return;
    }
    // Mark first: the UnmapNotify this causes must not unmanage the window.
    client->minimized = 1;
    XUnmapWindow(wm->display, client_toplevel(client));
    set_wm_state(wm, client->window, IconicState);
    ewmh_change_state(wm->display, client->window, NET_WM_STATE_ADD, noirAtoms[NET_WM_STATE_HIDDEN]);
    if (wm->activeWindow == client->window) {
        wm_focus_fallback(wm);
    }
}

void client_close(NoirWM* wm, NoirClient* client) {
    if (ewmh_supports_protocol(wm->display, client->window, noirAtoms[WM_DELETE_WINDOW])) {
        send_protocol(wm, client->window, noirAtoms[WM_DELETE_WINDOW]);
    } else {
        XKillClient(wm->display, client->window);
    }
}

void client_set_maximized(NoirWM* wm, NoirClient* client, int maximized) {
    if (!client->frame) {
        return;
    }
    if (maximized) {
        if (!client->maximized) {
            client->restoreX = client->x; client->restoreY = client->y;
            client->restoreWidth = client->width; client->restoreHeight = client->height;
        }
        long area[4];
        wm_get_workarea(wm, area);
        client->x = (int)area[0];
        client->y = (int)area[1];
        client->width = (int)area[2] - 2 * FRAME_BORDER;
        client->height = (int)area[3] - FRAME_TITLE - FRAME_BORDER;
    } else if (client->maximized) {
        client->x = client->restoreX; client->y = client->restoreY;
        client->width = client->restoreWidth; client->height = client->restoreHeight;
    }
    client->maximized = maximized;
    const long action = maximized ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE;
    ewmh_change_state(wm->display, client->window, action, noirAtoms[NET_WM_STATE_MAXIMIZED_VERT]);
    ewmh_change_state(wm->display, client->window, action, noirAtoms[NET_WM_STATE_MAXIMIZED_HORZ]);
    frame_apply_geometry(wm, client);
    frame_draw(wm, client);
}

void wm_cycle_focus(NoirWM* wm) {
    int start = -1;
    for (int i = 0; i < wm->clientCount; i++) {
        if (wm->clients[i].window == wm->activeWindow) {
            start = i;
        }
    }
    for (int step = 1; step <= wm->clientCount; step++) {
        NoirClient* c = &wm->clients[(start + step + wm->clientCount) % wm->clientCount];
        if (!c->isShellSurface && c->window != wm->activeWindow) {
            client_activate(wm, c);
            return;
        }
    }
}

// Current X stacking order of managed clients, bottom to top.
static int stacking_order(NoirWM* wm, Window* out) {
    Window rootReturn, parentReturn, *children = NULL;
    unsigned int count = 0;
    int n = 0;
    if (!XQueryTree(wm->display, wm->root, &rootReturn, &parentReturn, &children, &count)) {
        return 0;
    }
    for (unsigned int i = 0; i < count; i++) {
        // Decorated clients are on the root through their frame.
        NoirClient* client = client_find_frame(wm, children[i]);
        if (!client) {
            client = client_find(wm, children[i]);
        }
        if (client && n < wm->clientCount) {
            out[n++] = client->window;
        }
    }
    if (children) XFree(children);
    return n;
}

void wm_restack(NoirWM* wm) {
    if (wm->clientCount == 0) {
        return;
    }
    Window* current = malloc(sizeof(Window) * (size_t)wm->clientCount);
    Window* order = malloc(sizeof(Window) * (size_t)wm->clientCount);
    int count = stacking_order(wm, current);

    // XRestackWindows wants top to bottom: walk layers from the top down,
    // preserving the existing relative order inside each layer.
    int n = 0;
    for (int layer = LAYER_ABOVE; layer >= LAYER_DESKTOP; layer--) {
        for (int i = count - 1; i >= 0; i--) {
            NoirClient* c = client_find(wm, current[i]);
            if (c && (int)c->layer == layer) {
                order[n++] = client_toplevel(c);
            }
        }
    }
    if (n > 0) {
        XRaiseWindow(wm->display, order[0]);
        XRestackWindows(wm->display, order, n);
    }
    free(current);
    free(order);
    wm_update_client_lists(wm);
}

void wm_update_client_lists(NoirWM* wm) {
    Window* windows = malloc(sizeof(Window) * (size_t)(wm->clientCount + 1));
    for (int i = 0; i < wm->clientCount; i++) {
        windows[i] = wm->clients[i].window;
    }
    ewmh_set_windows(wm->display, wm->root, noirAtoms[NET_CLIENT_LIST], windows, wm->clientCount);
    int n = stacking_order(wm, windows);
    ewmh_set_windows(wm->display, wm->root, noirAtoms[NET_CLIENT_LIST_STACKING], windows, n);
    free(windows);
}

void wm_update_workarea(NoirWM* wm) {
    long reserved[4] = { 0 };  // left, right, top, bottom
    for (int i = 0; i < wm->clientCount; i++) {
        for (int s = 0; s < 4; s++) {
            if (wm->clients[i].strut[s] > reserved[s]) {
                reserved[s] = wm->clients[i].strut[s];
            }
        }
    }
    const long workarea[4] = {
        reserved[0],
        reserved[2],
        wm->screenWidth - reserved[0] - reserved[1],
        wm->screenHeight - reserved[2] - reserved[3],
    };
    ewmh_set_cardinals(wm->display, wm->root, noirAtoms[NET_WORKAREA], workarea, 4);
    // Maximized windows follow the work area (e.g. when the taskbar appears).
    for (int i = 0; i < wm->clientCount; i++) {
        if (wm->clients[i].maximized) {
            client_set_maximized(wm, &wm->clients[i], 1);
        }
    }
}

void wm_focus_fallback(NoirWM* wm) {
    Window* order = malloc(sizeof(Window) * (size_t)(wm->clientCount + 1));
    int count = stacking_order(wm, order);
    for (int i = count - 1; i >= 0; i--) {
        NoirClient* c = client_find(wm, order[i]);
        if (c && !c->minimized && !c->isShellSurface && (c->acceptsFocus || c->takesFocus) &&
            order[i] != wm->activeWindow) {
            free(order);
            client_activate(wm, c);
            return;
        }
    }
    free(order);
    XSetInputFocus(wm->display, PointerRoot, RevertToPointerRoot, CurrentTime);
    wm_set_active(wm, None);
}

void handle_map_request(NoirWM* wm, XMapRequestEvent* event) {
    XWindowAttributes wa;
    if (!XGetWindowAttributes(wm->display, event->window, &wa)) {
        return;
    }
    if (wa.override_redirect) {
        XMapWindow(wm->display, event->window);
        return;
    }
    NoirClient* client = client_manage(wm, event->window);
    if (!client) {
        XMapWindow(wm->display, event->window);
        return;
    }
    client->minimized = 0;
    XMapWindow(wm->display, event->window);
    if (client->frame) {
        XMapWindow(wm->display, client->frame);
    }
    set_wm_state(wm, event->window, NormalState);
    ewmh_change_state(wm->display, event->window, NET_WM_STATE_REMOVE, noirAtoms[NET_WM_STATE_HIDDEN]);

    // New application windows become the active one (and get the keyboard if
    // they want it); shell surfaces just take their layer.
    if (!client->isShellSurface) {
        client_activate(wm, client);
    } else {
        wm_restack(wm);
    }
}

void handle_configure_request(NoirWM* wm, XConfigureRequestEvent* event) {
    NoirClient* client = client_find(wm, event->window);
    if (client && client->frame) {
        // The frame is what moves; the client keeps its place inside it.
        if (!client->maximized) {
            if (event->value_mask & CWX) client->x = event->x;
            if (event->value_mask & CWY) client->y = event->y;
            if (event->value_mask & CWWidth) client->width = event->width;
            if (event->value_mask & CWHeight) client->height = event->height;
            frame_constrain_size(client, &client->width, &client->height);
        }
        frame_apply_geometry(wm, client);
        frame_draw(wm, client);
    } else {
        XWindowChanges ch;
        ch.x = event->x; ch.y = event->y;
        ch.width = event->width; ch.height = event->height;
        ch.border_width = event->border_width;
        ch.sibling = event->above; ch.stack_mode = event->detail;
        unsigned long mask = event->value_mask;
        if (client) {
            // NoirWM owns borders and stacking order for managed windows.
            mask &= ~(unsigned long)(CWBorderWidth | CWSibling | CWStackMode);
        }
        XConfigureWindow(wm->display, event->window, (unsigned int)mask, &ch);
    }
    if (client && (event->value_mask & CWStackMode) && event->detail == Above) {
        XRaiseWindow(wm->display, client_toplevel(client));
        wm_restack(wm);
    }
}
