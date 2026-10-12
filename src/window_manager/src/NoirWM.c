#include "NoirWM.h"
#include "Client.h"
#include "Events.h"
#include "Ewmh.h"
#include "Frame.h"
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

static volatile sig_atomic_t quitRequested = 0;

static void on_signal(int signal) {
    (void)signal;
    quitRequested = 1;
}

static int anotherWmRunning = 0;

static int on_wm_detected(Display* display, XErrorEvent* error) {
    (void)display;
    if (error->error_code == BadAccess) {
        anotherWmRunning = 1;
    }
    return 0;
}

// Clients can vanish between an event and our reply to it; a BadWindow then
// is expected and must not take the window manager down.
static int on_x_error(Display* display, XErrorEvent* error) {
    // Core errors below, plus anything from an extension (BadDamage and
    // friends, numbered from 128): all of them are a window going away.
    if (error->error_code == BadWindow || error->error_code == BadMatch ||
        error->error_code == BadDrawable || error->error_code == BadPixmap ||
        error->error_code >= 128) {
        return 0;
    }
    char text[256];
    XGetErrorText(display, error->error_code, text, sizeof(text));
    fprintf(stderr, "NoirWM: X error %s (request %d)\n", text, error->request_code);
    return 0;
}

static int window_is_iconic(NoirWM* wm, Window window) {
    Atom type;
    int format;
    unsigned long count = 0, remaining;
    unsigned char* data = NULL;
    int iconic = 0;
    if (XGetWindowProperty(wm->display, window, noirAtoms[WM_STATE], 0, 2, False, noirAtoms[WM_STATE],
                           &type, &format, &count, &remaining, &data) == Success && data) {
        iconic = count > 0 && ((long*)data)[0] == IconicState;
        XFree(data);
    }
    return iconic;
}

// Alt+Tab switches windows; Alt+F4 / F9 / F10 close, minimize and maximize
// the active one. Grabbed for every state of the lock modifiers.
static void grab_keys(NoirWM* wm) {
    const KeySym keys[] = { XK_Tab, XK_F4, XK_F9, XK_F10 };
    const unsigned int locks[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
        const KeyCode code = XKeysymToKeycode(wm->display, keys[k]);
        for (size_t l = 0; code && l < sizeof(locks) / sizeof(locks[0]); l++) {
            XGrabKey(wm->display, code, Mod1Mask | locks[l], wm->root, True, GrabModeAsync, GrabModeAsync);
        }
    }
}

// Adopt windows that were already mapped (or minimized) before NoirWM started.
static void manage_existing_windows(NoirWM* wm) {
    Window rootReturn, parentReturn, *children = NULL;
    unsigned int count = 0;
    if (!XQueryTree(wm->display, wm->root, &rootReturn, &parentReturn, &children, &count)) {
        return;
    }
    for (unsigned int i = 0; i < count; i++) {
        XWindowAttributes wa;
        if (children[i] == wm->checkWindow || !XGetWindowAttributes(wm->display, children[i], &wa)) {
            continue;
        }
        if (wa.override_redirect) {
            continue;
        }
        if (wa.map_state == IsViewable) {
            client_manage(wm, children[i]);
        } else if (window_is_iconic(wm, children[i])) {
            // Minimized under a previous WM instance: keep it on the taskbar.
            NoirClient* client = client_manage(wm, children[i]);
            if (client) {
                client->minimized = 1;
                const long iconic[2] = { IconicState, None };
                XChangeProperty(wm->display, children[i], noirAtoms[WM_STATE], noirAtoms[WM_STATE], 32,
                                PropModeReplace, (const unsigned char*)iconic, 2);
                ewmh_change_state(wm->display, children[i], NET_WM_STATE_ADD, noirAtoms[NET_WM_STATE_HIDDEN]);
            }
        }
    }
    if (children) XFree(children);
    wm_restack(wm);
}

NoirWM* NoirWM_create(int useCompositor) {
    NoirWM* wm = (NoirWM*)calloc(1, sizeof(NoirWM));
    if (!wm) return NULL;

    wm->display = XOpenDisplay(NULL);
    if (!wm->display) {
        fprintf(stderr, "Cannot open display\n");
        free(wm);
        return NULL;
    }

    const int screen = DefaultScreen(wm->display);
    wm->root = RootWindow(wm->display, screen);
    wm->screenWidth = DisplayWidth(wm->display, screen);
    wm->screenHeight = DisplayHeight(wm->display, screen);

    // Only one client may select SubstructureRedirect on the root.
    XSetErrorHandler(on_wm_detected);
    XSelectInput(wm->display, wm->root, SubstructureRedirectMask | SubstructureNotifyMask | PropertyChangeMask);
    XSync(wm->display, False);
    if (anotherWmRunning) {
        fprintf(stderr, "Another window manager is already running\n");
        XCloseDisplay(wm->display);
        free(wm);
        return NULL;
    }
    XSetErrorHandler(on_x_error);

    ewmh_init_atoms(wm->display);
    frame_init(wm);
    grab_keys(wm);

    // Before the EWMH announcement below: clients that wait for the window
    // manager then also find the compositing manager selection in place.
    if (useCompositor) {
        wm->compositor = compositor_create(wm->display, wm->root);
        if (wm->compositor) {
            fprintf(stderr, "NoirWM: compositing with Vulkan on %s\n", compositor_device_name(wm->compositor));
        } else {
            fprintf(stderr, "NoirWM: compositor unavailable, managing windows without it\n");
        }
    }
    wm->checkWindow = ewmh_setup_root(wm->display, wm->root, wm->screenWidth, wm->screenHeight);

    manage_existing_windows(wm);
    XSync(wm->display, False);
    return wm;
}

void NoirWM_run(NoirWM* wm) {
    struct sigaction action = { 0 };
    action.sa_handler = on_signal;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    XEvent event;
    while (!quitRequested) {
        while (XPending(wm->display) > 0) {
            XNextEvent(wm->display, &event);
            // The compositor mirrors every top-level window, managed or not.
            if (wm->compositor) {
                compositor_handle_event(wm->compositor, &event);
            }
            handle_event(wm, &event);
        }
        // Sleep until the X server has something for us, or until the next
        // frame is due when there is one to draw.
        int timeout = -1;
        if (wm->compositor && compositor_needs_frame(wm->compositor)) {
            timeout = compositor_frame_delay(wm->compositor);
            if (timeout == 0) {
                compositor_draw(wm->compositor);
                continue;
            }
        }
        XFlush(wm->display);
        struct pollfd fd = { ConnectionNumber(wm->display), POLLIN, 0 };
        poll(&fd, 1, timeout);
    }
}

void NoirWM_destroy(NoirWM* wm) {
    if (wm) {
        // Hand every client back to the root before the frames go away.
        for (int i = 0; i < wm->clientCount; i++) {
            const int wasMinimized = wm->clients[i].minimized;
            frame_destroy(wm, &wm->clients[i]);
            if (!wasMinimized) {
                XMapWindow(wm->display, wm->clients[i].window);
            }
        }
        if (wm->compositor) {
            compositor_destroy(wm->compositor);
        }
        frame_shutdown(wm);
        if (wm->checkWindow) {
            XDestroyWindow(wm->display, wm->checkWindow);
        }
        XSync(wm->display, False);
        free(wm->clients);
        XCloseDisplay(wm->display);
        free(wm);
    }
}
