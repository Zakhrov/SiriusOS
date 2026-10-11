#pragma once

#include <X11/Xlib.h>
#include <VulkanRenderer.h>

// Stacking layers, bottom to top.
typedef enum {
    LAYER_DESKTOP,
    LAYER_BELOW,
    LAYER_NORMAL,
    LAYER_ABOVE,
} NoirLayer;

typedef struct {
    Window window;
    NoirLayer layer;
    int minimized;      // iconified by us: unmapped but still managed
    int acceptsFocus;   // WM_HINTS input: eligible for XSetInputFocus
    int takesFocus;     // WM_PROTOCOLS includes WM_TAKE_FOCUS
    int focusOnClick;   // user interaction may focus the window
    int isShellSurface; // desktop, dock, menu, or notification surface
    int isDock;
    int decorated;      // gets the silver border
    long strut[4];      // left, right, top, bottom reserved by this client
} NoirClient;

typedef struct {
    Display* display;
    Window root;
    int screenWidth;
    int screenHeight;
    VulkanRenderer* renderer;  // NULL when running without the compositor
    Window checkWindow;
    Window activeWindow;
    NoirClient* clients;       // in mapping order (_NET_CLIENT_LIST order)
    int clientCount;
    int clientCapacity;
} NoirWM;

NoirWM* NoirWM_create(int useCompositor);
void NoirWM_destroy(NoirWM* wm);
void NoirWM_run(NoirWM* wm);
