#include "NoirWM.h"
#include "Client.h"
#include <stdlib.h>
#include <stdio.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>


NoirWM* NoirWM_create() {
    NoirWM* wm = (NoirWM*)malloc(sizeof(NoirWM));
    if (!wm) return NULL;

    wm->display = XOpenDisplay(NULL);
    if (!wm->display) {
        fprintf(stderr, "Cannot open display\n");
        free(wm);
        return NULL;
    }

    wm->root = DefaultRootWindow(wm->display);
    XSelectInput(wm->display, wm->root, SubstructureRedirectMask | SubstructureNotifyMask);
    int eventBase, errorBase;
    if (!XCompositeQueryExtension(wm->display, &eventBase, &errorBase)) {
        fprintf(stderr, "XComposite not available\n");
        return NULL;
    }
    XCompositeRedirectSubwindows(wm->display, wm->root, CompositeRedirectManual);

    // Init XDamage
    int damageEvent, damageError;
    if (!XDamageQueryExtension(wm->display, &damageEvent, &damageError)) {
        fprintf(stderr, "XDamage not available\n");
        return NULL;
    }
    wm->renderer = createVulkanRenderer(wm->display, wm->root);
    if (!wm->renderer) {
        fprintf(stderr, "Failed to create VulkanRenderer\n");
        XCloseDisplay(wm->display);
        free(wm);
        return NULL;
    }

    return wm;
}

void NoirWM_run(NoirWM* wm) {
    XEvent event;
    while (1) {
        while (XPending(wm->display) > 0) {
            XNextEvent(wm->display, &event);
            switch (event.type) {
            case MapRequest:
                handle_map_request(wm, &event.xmaprequest);
                break;
            case ConfigureRequest:
                handle_configure_request(wm, &event.xconfigurerequest);
                break;
            default:
                break;
            }
        }
        drawVulkanRendererFrame(wm->renderer);


    }
}

void NoirWM_destroy(NoirWM* wm) {
    if (wm) {
        XCloseDisplay(wm->display);
        free(wm);
    }
}