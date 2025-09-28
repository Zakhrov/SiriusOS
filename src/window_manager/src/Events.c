#include "Events.h"
#include "Client.h"

void handle_event(NoirWM* wm, XEvent* event) {
    switch (event->type) {
    case MapRequest:
        handle_map_request(wm, &event->xmaprequest);
        break;
    case ConfigureRequest:
        handle_configure_request(wm, &event->xconfigurerequest);
        break;
    default:
        // Optional: print or ignore
        break;
    }
}
