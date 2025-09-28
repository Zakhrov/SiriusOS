#include "Client.h"

void handle_map_request(NoirWM* wm, XMapRequestEvent* event) {
    XWindowAttributes wa;
    XGetWindowAttributes(wm->display, event->window, &wa);
    if (!wa.override_redirect) {
        XSetWindowBorderWidth(wm->display, event->window, 1);
    }
    XMapWindow(wm->display, event->window);
}

void handle_configure_request(NoirWM* wm, XConfigureRequestEvent* event) {
    XWindowChanges ch;
    ch.x = event->x; ch.y = event->y;
    ch.width = event->width; ch.height = event->height;
    ch.border_width = event->border_width;
    ch.sibling = event->above; ch.stack_mode = event->detail;
    XConfigureWindow(wm->display, event->window, event->value_mask, &ch);
}