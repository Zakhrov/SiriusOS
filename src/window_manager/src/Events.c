#include "Events.h"
#include "Client.h"
#include "Ewmh.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

static void handle_unmap_notify(NoirWM* wm, XUnmapEvent* event) {
    NoirClient* client = client_find(wm, event->window);
    // Our own minimize unmaps the window too; only a client-initiated unmap
    // (ICCCM withdraw) ends management.
    if (!client || client->minimized) {
        return;
    }
    const long withdrawn[2] = { WithdrawnState, None };
    XChangeProperty(wm->display, event->window, noirAtoms[WM_STATE], noirAtoms[WM_STATE], 32,
                    PropModeReplace, (const unsigned char*)withdrawn, 2);
    client_unmanage(wm, event->window);
}

static void handle_client_message(NoirWM* wm, XClientMessageEvent* event) {
    NoirClient* client = client_find(wm, event->window);
    if (!client) {
        return;
    }
    const Atom type = event->message_type;
    if (type == noirAtoms[NET_ACTIVE_WINDOW]) {
        // data.l[0] is the source: 1 = application, 2 = pager/taskbar. Toolkits
        // ask to activate a dock when it is clicked; honouring that would make
        // the taskbar the active window and break its minimize-on-click.
        const long source = event->data.l[0];
        if (client->isDock && source != 2) {
            return;
        }
        client_activate(wm, client);
    } else if (type == noirAtoms[NET_CLOSE_WINDOW]) {
        client_close(wm, client);
    } else if (type == noirAtoms[WM_CHANGE_STATE]) {
        if (event->data.l[0] == IconicState) {
            client_minimize(wm, client);
        }
    } else if (type == noirAtoms[NET_WM_STATE]) {
        // data.l[0] = action, data.l[1..2] = up to two state atoms.
        int restack = 0;
        for (int i = 1; i <= 2; i++) {
            const Atom state = (Atom)event->data.l[i];
            if (state == None || state == noirAtoms[NET_WM_STATE_HIDDEN]) {
                continue;  // HIDDEN is owned by the WM (see WM_CHANGE_STATE)
            }
            if (state == noirAtoms[NET_WM_STATE_ABOVE] || state == noirAtoms[NET_WM_STATE_BELOW] ||
                state == noirAtoms[NET_WM_STATE_SKIP_TASKBAR] || state == noirAtoms[NET_WM_STATE_SKIP_PAGER] ||
                state == noirAtoms[NET_WM_STATE_STICKY] || state == noirAtoms[NET_WM_STATE_DEMANDS_ATTENTION]) {
                ewmh_change_state(wm->display, client->window, event->data.l[0], state);
                restack = 1;
            }
        }
        if (restack) {
            client_update_type(wm, client);
        }
    }
}

static void handle_property_notify(NoirWM* wm, XPropertyEvent* event) {
    NoirClient* client = client_find(wm, event->window);
    if (!client || event->state == PropertyDelete) {
        return;
    }
    if (event->atom == noirAtoms[NET_WM_STRUT] || event->atom == noirAtoms[NET_WM_STRUT_PARTIAL]) {
        client_update_strut(wm, client);
    } else if (event->atom == noirAtoms[NET_WM_WINDOW_TYPE]) {
        client_update_type(wm, client);
    } else if (event->atom == XA_WM_HINTS) {
        // ICCCM urgency maps onto _NET_WM_STATE_DEMANDS_ATTENTION for the taskbar.
        XWMHints* hints = XGetWMHints(wm->display, client->window);
        if (hints) {
            const int urgent = (hints->flags & XUrgencyHint) != 0 && client->window != wm->activeWindow;
            ewmh_change_state(wm->display, client->window,
                              urgent ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE,
                              noirAtoms[NET_WM_STATE_DEMANDS_ATTENTION]);
            XFree(hints);
        }
    }
}

static void handle_button_press(NoirWM* wm, XButtonEvent* event) {
    NoirClient* client = client_find(wm, event->window);
    if (client && client->focusOnClick && client->window != wm->activeWindow) {
        client_activate(wm, client);
    }
    // Hand the click on to the application.
    XAllowEvents(wm->display, ReplayPointer, event->time);
}

void handle_event(NoirWM* wm, XEvent* event) {
    switch (event->type) {
    case MapRequest:
        handle_map_request(wm, &event->xmaprequest);
        break;
    case ConfigureRequest:
        handle_configure_request(wm, &event->xconfigurerequest);
        break;
    case UnmapNotify:
        handle_unmap_notify(wm, &event->xunmap);
        break;
    case DestroyNotify:
        client_unmanage(wm, event->xdestroywindow.window);
        break;
    case ClientMessage:
        handle_client_message(wm, &event->xclient);
        break;
    case PropertyNotify:
        handle_property_notify(wm, &event->xproperty);
        break;
    case ButtonPress:
        handle_button_press(wm, &event->xbutton);
        break;
    default:
        break;
    }
}
