#pragma once

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <Compositor.h>

// Stacking layers, bottom to top.
typedef enum {
    LAYER_DESKTOP,
    LAYER_BELOW,
    LAYER_NORMAL,
    LAYER_ABOVE,
} NoirLayer;

#define NOIR_ICON_SIZE 16

typedef struct {
    Window window;
    Window frame;       // decoration window the client is reparented into; None if undecorated
    XftDraw* draw;      // title text target on the frame
    NoirLayer layer;
    int minimized;      // iconified by us: unmapped but still managed
    int maximized;      // fills the work area; restore* holds the previous geometry
    int acceptsFocus;   // WM_HINTS input: eligible for XSetInputFocus
    int takesFocus;     // WM_PROTOCOLS includes WM_TAKE_FOCUS
    int focusOnClick;   // user interaction may focus the window
    int isShellSurface; // desktop, dock, menu, or notification surface
    int isDock;
    int decorated;      // gets a frame with a title bar
    // Decorated clients only: frame position on the root and client area size.
    int x, y, width, height;
    int restoreX, restoreY, restoreWidth, restoreHeight;
    // WM_NORMAL_HINTS (0 = unset).
    int minWidth, minHeight, maxWidth, maxHeight;
    int baseWidth, baseHeight, incWidth, incHeight;
    char title[256];
    int hasIcon;
    unsigned int icon[NOIR_ICON_SIZE * NOIR_ICON_SIZE];   // ARGB, straight alpha
    int hoverButton;    // title bar button under the pointer (a FrameHit), or 0
    int pressedButton;  // title bar button being clicked, or 0
    long strut[4];      // left, right, top, bottom reserved by this client
} NoirClient;

typedef enum {
    DRAG_NONE,
    DRAG_MOVE,
    DRAG_RESIZE,
} NoirDragMode;

typedef struct {
    Display* display;
    Window root;
    int screenWidth;
    int screenHeight;
    NoirCompositor* compositor;  // NULL when running without compositing
    Window checkWindow;
    Window activeWindow;
    NoirClient* clients;       // in mapping order (_NET_CLIENT_LIST order)
    int clientCount;
    int clientCapacity;

    // Decorations.
    XftFont* font;
    Cursor cursors[16];        // indexed by a mask of FRAME_EDGE_* bits; 0 is the arrow
    int placementStep;         // cascade position for the next new window

    // Interactive move / resize in progress.
    NoirDragMode dragMode;
    Window dragClient;
    int dragEdges;
    int dragStartX, dragStartY;                     // pointer, root coordinates
    int dragX, dragY, dragWidth, dragHeight;        // client geometry at the start
    Window lastClickClient;                         // for double-click on a title bar
    Time lastClickTime;
} NoirWM;

// Tries to start the Vulkan compositor when asked to; falls back to plain
// window management if that fails.
NoirWM* NoirWM_create(int useCompositor);
void NoirWM_destroy(NoirWM* wm);
void NoirWM_run(NoirWM* wm);
