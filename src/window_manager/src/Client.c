#include "Client.h"
#include "Ewmh.h"

#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>

#define BORDER_WIDTH 1
#define BORDER_SILVER 0xc3c7cf

NoirClient* client_find(NoirWM* wm, Window window) {
    for (int i = 0; i < wm->clientCount; i++) {
        if (wm->clients[i].window == window) {
            return &wm->clients[i];
        }
    }
    return NULL;
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

    client->decorated = !(isDesktop || isDock || isOverlay || isUndecorated);
    client->focusOnClick = !(isDock || isOverlay);
    client->isDock = isDock;

    // ICCCM: a client that sets WM_HINTS input=False and does not speak
    // WM_TAKE_FOCUS never wants the keyboard.
    client->acceptsFocus = 1;
    XWMHints* hints = XGetWMHints(d, client->window);
    if (hints) {
        if ((hints->flags & InputHint) && !hints->input &&
            !ewmh_supports_protocol(d, client->window, noirAtoms[WM_TAKE_FOCUS])) {
            client->acceptsFocus = 0;
        }
        XFree(hints);
    }
}

static void apply_decoration(NoirWM* wm, NoirClient* client) {
    const int border = client->decorated ? BORDER_WIDTH : 0;
    XSetWindowBorderWidth(wm->display, client->window, border);
    if (border && DefaultDepth(wm->display, DefaultScreen(wm->display)) >= 24) {
        XSetWindowBorder(wm->display, client->window, BORDER_SILVER);
    }
    const long extents[4] = { border, border, border, border };
    ewmh_set_cardinals(wm->display, client->window, noirAtoms[NET_FRAME_EXTENTS], extents, 4);
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
    apply_decoration(wm, client);
    wm_restack(wm);
}

// Keep new normal windows inside the work area (i.e. clear of the taskbar).
static void place_client(NoirWM* wm, NoirClient* client) {
    if (client->layer != LAYER_NORMAL) {
        return;
    }
    long area[4] = { 0, 0, wm->screenWidth, wm->screenHeight };
    ewmh_get_cardinals(wm->display, wm->root, noirAtoms[NET_WORKAREA], area, 4);

    XWindowAttributes wa;
    if (!XGetWindowAttributes(wm->display, client->window, &wa)) {
        return;
    }
    const int border = client->decorated ? BORDER_WIDTH : 0;
    const int w = wa.width + 2 * border, h = wa.height + 2 * border;
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

    read_type(wm, client);
    apply_decoration(wm, client);

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
    wm->activeWindow = window;
    ewmh_set_windows(wm->display, wm->root, noirAtoms[NET_ACTIVE_WINDOW], &window, 1);
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
        XMapWindow(d, client->window);
        set_wm_state(wm, client->window, NormalState);
        ewmh_change_state(d, client->window, NET_WM_STATE_REMOVE, noirAtoms[NET_WM_STATE_HIDDEN]);
    }
    ewmh_change_state(d, client->window, NET_WM_STATE_REMOVE, noirAtoms[NET_WM_STATE_DEMANDS_ATTENTION]);
    if (client->layer != LAYER_DESKTOP) {
        XRaiseWindow(d, client->window);
    }
    wm_restack(wm);

    if (client->acceptsFocus) {
        XSetInputFocus(d, client->window, RevertToPointerRoot, CurrentTime);
    }
    if (ewmh_supports_protocol(d, client->window, noirAtoms[WM_TAKE_FOCUS])) {
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
    XUnmapWindow(wm->display, client->window);
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

// Current X stacking order of managed clients, bottom to top.
static int stacking_order(NoirWM* wm, Window* out) {
    Window rootReturn, parentReturn, *children = NULL;
    unsigned int count = 0;
    int n = 0;
    if (!XQueryTree(wm->display, wm->root, &rootReturn, &parentReturn, &children, &count)) {
        return 0;
    }
    for (unsigned int i = 0; i < count; i++) {
        if (client_find(wm, children[i])) {
            out[n++] = children[i];
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
                order[n++] = current[i];
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
}

void wm_focus_fallback(NoirWM* wm) {
    Window* order = malloc(sizeof(Window) * (size_t)(wm->clientCount + 1));
    int count = stacking_order(wm, order);
    for (int i = count - 1; i >= 0; i--) {
        NoirClient* c = client_find(wm, order[i]);
        if (c && !c->minimized && c->layer == LAYER_NORMAL && c->acceptsFocus &&
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
    set_wm_state(wm, event->window, NormalState);
    ewmh_change_state(wm->display, event->window, NET_WM_STATE_REMOVE, noirAtoms[NET_WM_STATE_HIDDEN]);

    // New application windows get focus; shell surfaces just take their layer.
    if (client->layer == LAYER_NORMAL && client->acceptsFocus) {
        client_activate(wm, client);
    } else {
        wm_restack(wm);
    }
}

void handle_configure_request(NoirWM* wm, XConfigureRequestEvent* event) {
    XWindowChanges ch;
    ch.x = event->x; ch.y = event->y;
    ch.width = event->width; ch.height = event->height;
    ch.border_width = event->border_width;
    ch.sibling = event->above; ch.stack_mode = event->detail;

    unsigned long mask = event->value_mask;
    NoirClient* client = client_find(wm, event->window);
    if (client) {
        // NoirWM owns borders and stacking order for managed windows.
        mask &= ~(unsigned long)(CWBorderWidth | CWSibling | CWStackMode);
    }
    XConfigureWindow(wm->display, event->window, (unsigned int)mask, &ch);
    if (client && (event->value_mask & CWStackMode) && event->detail == Above) {
        XRaiseWindow(wm->display, event->window);
        wm_restack(wm);
    }
}
