#include "Events.h"
#include "Client.h"
#include "Ewmh.h"
#include "Frame.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

static void handle_unmap_notify(NoirWM* wm, XUnmapEvent* event) {
    NoirClient* client = client_find(wm, event->window);
    if (!client) {
        return;
    }
    if (client->frame) {
        // Reparenting into the frame unmaps the client as seen from the root;
        // only an unmap inside the frame (or the ICCCM synthetic one) is a
        // withdrawal.
        if (event->event != client->frame && !event->send_event) {
            return;
        }
    } else if (client->minimized) {
        // Our own minimize unmaps a frameless window too.
        return;
    }
    const long withdrawn[2] = { WithdrawnState, None };
    XChangeProperty(wm->display, event->window, noirAtoms[WM_STATE], noirAtoms[WM_STATE], 32,
                    PropModeReplace, (const unsigned char*)withdrawn, 2);
    client_unmanage(wm, event->window);
}

static void handle_client_message(NoirWM* wm, XClientMessageEvent* event) {
    const Atom type = event->message_type;
    if (type == noirAtoms[NET_REQUEST_FRAME_EXTENTS]) {
        // Asked before mapping, so the client can size itself: every window
        // we would decorate gets the same frame.
        const long extents[4] = { FRAME_BORDER, FRAME_BORDER, FRAME_TITLE, FRAME_BORDER };
        ewmh_set_cardinals(wm->display, event->window, noirAtoms[NET_FRAME_EXTENTS], extents, 4);
        return;
    }
    NoirClient* client = client_find(wm, event->window);
    if (!client) {
        return;
    }
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
        int restack = 0, maximize = 0;
        for (int i = 1; i <= 2; i++) {
            const Atom state = (Atom)event->data.l[i];
            if (state == None || state == noirAtoms[NET_WM_STATE_HIDDEN]) {
                continue;  // HIDDEN is owned by the WM (see WM_CHANGE_STATE)
            }
            if (state == noirAtoms[NET_WM_STATE_MAXIMIZED_VERT] || state == noirAtoms[NET_WM_STATE_MAXIMIZED_HORZ]) {
                maximize = 1;
            }
            if (state == noirAtoms[NET_WM_STATE_ABOVE] || state == noirAtoms[NET_WM_STATE_BELOW] ||
                state == noirAtoms[NET_WM_STATE_SKIP_TASKBAR] || state == noirAtoms[NET_WM_STATE_SKIP_PAGER] ||
                state == noirAtoms[NET_WM_STATE_STICKY] || state == noirAtoms[NET_WM_STATE_DEMANDS_ATTENTION]) {
                ewmh_change_state(wm->display, client->window, event->data.l[0], state);
                restack = 1;
            }
        }
        if (maximize) {
            // NoirWM maximizes in both directions at once.
            const long action = event->data.l[0];
            client_set_maximized(wm, client, action == NET_WM_STATE_TOGGLE ? !client->maximized
                                                                           : action == NET_WM_STATE_ADD);
        }
        if (restack) {
            client_update_type(wm, client);
        }
    }
}

static void handle_property_notify(NoirWM* wm, XPropertyEvent* event) {
    NoirClient* client = client_find(wm, event->window);
    if (!client) {
        return;
    }
    if (event->atom == noirAtoms[NET_WM_STRUT] || event->atom == noirAtoms[NET_WM_STRUT_PARTIAL]) {
        client_update_strut(wm, client);
    } else if (event->atom == noirAtoms[NET_WM_WINDOW_TYPE] || event->atom == noirAtoms[MOTIF_WM_HINTS]) {
        client_update_type(wm, client);
    } else if (event->atom == noirAtoms[NET_WM_NAME] || event->atom == XA_WM_NAME) {
        frame_read_title(wm, client);
        frame_draw(wm, client);
    } else if (event->atom == noirAtoms[NET_WM_ICON]) {
        frame_read_icon(wm, client);
        frame_draw(wm, client);
    } else if (event->atom == XA_WM_NORMAL_HINTS) {
        frame_read_size_hints(wm, client);
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

// Global shortcuts, grabbed on the root in NoirWM_create.
static void handle_key_press(NoirWM* wm, XKeyEvent* event) {
    const KeySym key = XLookupKeysym(event, 0);
    NoirClient* active = client_find(wm, wm->activeWindow);
    if (key == XK_Tab) {
        wm_cycle_focus(wm);
    } else if (!active) {
        return;
    } else if (key == XK_F4) {
        client_close(wm, active);
    } else if (key == XK_F9) {
        client_minimize(wm, active);
    } else if (key == XK_F10) {
        client_set_maximized(wm, active, !active->maximized);
    }
}

static void handle_button_press(NoirWM* wm, XButtonEvent* event) {
    NoirClient* framed = client_find_frame(wm, event->window);
    if (framed) {
        frame_handle_button_press(wm, framed, event);
        return;
    }
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
    case ButtonRelease:
        frame_handle_button_release(wm, &event->xbutton);
        break;
    case MotionNotify:
        frame_handle_motion(wm, &event->xmotion);
        break;
    case LeaveNotify: {
        NoirClient* client = client_find_frame(wm, event->xcrossing.window);
        if (client) {
            frame_handle_leave(wm, client);
        }
        break;
    }
    case Expose: {
        NoirClient* client = event->xexpose.count == 0 ? client_find_frame(wm, event->xexpose.window) : NULL;
        if (client) {
            frame_draw(wm, client);
        }
        break;
    }
    case KeyPress:
        handle_key_press(wm, &event->xkey);
        break;
    default:
        break;
    }
}
