#include "NoirWM.h"
#include "Client.h"
#include "Events.h"
#include "Ewmh.h"
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>

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
    if (error->error_code == BadWindow || error->error_code == BadMatch ||
        error->error_code == BadDrawable) {
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
    wm->checkWindow = ewmh_setup_root(wm->display, wm->root, wm->screenWidth, wm->screenHeight);

    if (useCompositor) {
        int eventBase, errorBase;
        if (!XCompositeQueryExtension(wm->display, &eventBase, &errorBase)) {
            fprintf(stderr, "XComposite not available\n");
            NoirWM_destroy(wm);
            return NULL;
        }
        XCompositeRedirectSubwindows(wm->display, wm->root, CompositeRedirectManual);

        int damageEvent, damageError;
        if (!XDamageQueryExtension(wm->display, &damageEvent, &damageError)) {
            fprintf(stderr, "XDamage not available\n");
            NoirWM_destroy(wm);
            return NULL;
        }
        wm->renderer = createVulkanRenderer(wm->display, wm->root);
        if (!wm->renderer) {
            fprintf(stderr, "Failed to create VulkanRenderer\n");
            NoirWM_destroy(wm);
            return NULL;
        }
    }

    manage_existing_windows(wm);
    XSync(wm->display, False);
    return wm;
}

void NoirWM_run(NoirWM* wm) {
    XEvent event;
    while (1) {
        if (!wm->renderer) {
            // No compositor: just block on X events.
            XNextEvent(wm->display, &event);
            handle_event(wm, &event);
            continue;
        }
        while (XPending(wm->display) > 0) {
            XNextEvent(wm->display, &event);
            handle_event(wm, &event);
        }
        drawVulkanRendererFrame(wm->renderer);
    }
}

void NoirWM_destroy(NoirWM* wm) {
    if (wm) {
        if (wm->renderer) {
            destroyVulkanRenderer(wm->renderer);
        }
        if (wm->checkWindow) {
            XDestroyWindow(wm->display, wm->checkWindow);
        }
        free(wm->clients);
        XCloseDisplay(wm->display);
        free(wm);
    }
}
