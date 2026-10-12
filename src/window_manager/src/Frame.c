#include "Frame.h"
#include "Client.h"
#include "Ewmh.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Noir palette (see VDD.md and the shell's Theme.qml).
#define COLOR_PANEL        0x121317
#define COLOR_PANEL_DIM    0x0a0a0c
#define COLOR_SILVER       0xc3c7cf
#define COLOR_SILVER_DIM   0x6c7079
#define COLOR_SILVER_FAINT 0x2b2d33
#define COLOR_WHITE        0xf4f6fa
#define COLOR_BLACK        0x050506

#define CORNER_SIZE 14            // how far a corner's resize zone reaches along an edge
#define MIN_FRAME_WIDTH 180
#define DOUBLE_CLICK_MS 400

// What a point on the frame belongs to.
typedef enum {
    HIT_NONE,
    HIT_TITLE,
    HIT_MINIMIZE,
    HIT_MAXIMIZE,
    HIT_CLOSE,
    HIT_RESIZE,
} FrameHit;

static int frame_width(const NoirClient* c) { return c->width + 2 * FRAME_BORDER; }
static int frame_height(const NoirClient* c) { return c->height + FRAME_TITLE + FRAME_BORDER; }
// Left edge of the button group; the buttons sit flush against the outline.
static int buttons_left(const NoirClient* c) { return frame_width(c) - 1 - 3 * FRAME_BUTTON_WIDTH; }

static FrameHit hit_test(const NoirClient* c, int x, int y, int* edges) {
    const int fw = frame_width(c), fh = frame_height(c);
    *edges = 0;
    if (y >= 1 && y < FRAME_TITLE && x >= buttons_left(c) && x < fw - 1) {
        return HIT_MINIMIZE + (x - buttons_left(c)) / FRAME_BUTTON_WIDTH;
    }
    if (!c->maximized) {
        int left = x < FRAME_BORDER, right = x >= fw - FRAME_BORDER;
        int top = y < FRAME_BORDER, bottom = y >= fh - FRAME_BORDER;
        if (left || right || top || bottom) {
            // Near a corner, an edge resizes in both directions.
            if (top || bottom) {
                left |= x < CORNER_SIZE;
                right |= x >= fw - CORNER_SIZE;
            }
            if (left || right) {
                top |= y < CORNER_SIZE;
                bottom |= y >= fh - CORNER_SIZE;
            }
            *edges = (left ? FRAME_EDGE_LEFT : 0) | (right ? FRAME_EDGE_RIGHT : 0) |
                     (top ? FRAME_EDGE_TOP : 0) | (bottom ? FRAME_EDGE_BOTTOM : 0);
            return HIT_RESIZE;
        }
    }
    return y < FRAME_TITLE ? HIT_TITLE : HIT_NONE;
}

// --- setup -------------------------------------------------------------------

void frame_init(NoirWM* wm) {
    Display* d = wm->display;
    const int screen = DefaultScreen(d);
    wm->font = XftFontOpenName(d, screen, "Roboto:size=10:weight=medium");
    if (!wm->font) {
        wm->font = XftFontOpenName(d, screen, "sans-10");
    }

    static const struct { int edges; unsigned int shape; } shapes[] = {
        { 0, XC_left_ptr },
        { FRAME_EDGE_LEFT, XC_left_side }, { FRAME_EDGE_RIGHT, XC_right_side },
        { FRAME_EDGE_TOP, XC_top_side }, { FRAME_EDGE_BOTTOM, XC_bottom_side },
        { FRAME_EDGE_TOP | FRAME_EDGE_LEFT, XC_top_left_corner },
        { FRAME_EDGE_TOP | FRAME_EDGE_RIGHT, XC_top_right_corner },
        { FRAME_EDGE_BOTTOM | FRAME_EDGE_LEFT, XC_bottom_left_corner },
        { FRAME_EDGE_BOTTOM | FRAME_EDGE_RIGHT, XC_bottom_right_corner },
    };
    for (size_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); i++) {
        wm->cursors[shapes[i].edges] = XCreateFontCursor(d, shapes[i].shape);
    }
    // Without this the pointer is the X server's default cross over the root.
    XDefineCursor(d, wm->root, wm->cursors[0]);
}

void frame_shutdown(NoirWM* wm) {
    if (wm->font) {
        XftFontClose(wm->display, wm->font);
        wm->font = NULL;
    }
    for (int i = 0; i < 16; i++) {
        if (wm->cursors[i]) {
            XFreeCursor(wm->display, wm->cursors[i]);
        }
    }
}

void frame_create(NoirWM* wm, NoirClient* c) {
    Display* d = wm->display;
    XSetWindowAttributes attrs;
    attrs.background_pixel = COLOR_PANEL_DIM;
    attrs.border_pixel = 0;
    attrs.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ExposureMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask | LeaveWindowMask;
    c->frame = XCreateWindow(d, wm->root, c->x, c->y,
                             (unsigned int)frame_width(c), (unsigned int)frame_height(c), 0,
                             CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWBorderPixel | CWEventMask, &attrs);
    const int screen = DefaultScreen(d);
    c->draw = XftDrawCreate(d, c->frame, DefaultVisual(d, screen), DefaultColormap(d, screen));

    // If NoirWM dies, the server hands the client back to the root for us.
    XAddToSaveSet(d, c->window);
    XSetWindowBorderWidth(d, c->window, 0);
    XReparentWindow(d, c->window, c->frame, FRAME_BORDER, FRAME_TITLE);
    XResizeWindow(d, c->window, (unsigned int)c->width, (unsigned int)c->height);

    const long extents[4] = { FRAME_BORDER, FRAME_BORDER, FRAME_TITLE, FRAME_BORDER };
    ewmh_set_cardinals(d, c->window, noirAtoms[NET_FRAME_EXTENTS], extents, 4);
    const Atom actions[] = {
        noirAtoms[NET_WM_ACTION_MOVE], noirAtoms[NET_WM_ACTION_RESIZE], noirAtoms[NET_WM_ACTION_MINIMIZE],
        noirAtoms[NET_WM_ACTION_MAXIMIZE_VERT], noirAtoms[NET_WM_ACTION_MAXIMIZE_HORZ],
        noirAtoms[NET_WM_ACTION_CLOSE],
    };
    ewmh_set_atoms(d, c->window, noirAtoms[NET_WM_ALLOWED_ACTIONS], actions, 6);

    if (wm->compositor) {
        compositor_set_shadow(wm->compositor, c->frame, 1);
    }
}

void frame_destroy(NoirWM* wm, NoirClient* c) {
    if (!c->frame) {
        return;
    }
    Display* d = wm->display;
    // Harmless (an ignored BadWindow) when the client is already gone.
    XReparentWindow(d, c->window, wm->root, c->x + FRAME_BORDER, c->y + FRAME_TITLE);
    XRemoveFromSaveSet(d, c->window);
    if (c->draw) {
        XftDrawDestroy(c->draw);
        c->draw = NULL;
    }
    XDestroyWindow(d, c->frame);
    c->frame = None;
}

// --- client properties -------------------------------------------------------

void frame_read_title(NoirWM* wm, NoirClient* c) {
    Display* d = wm->display;
    c->title[0] = '\0';
    Atom type;
    int format;
    unsigned long count = 0, remaining;
    unsigned char* data = NULL;
    if (XGetWindowProperty(d, c->window, noirAtoms[NET_WM_NAME], 0, 256, False, noirAtoms[UTF8_STRING],
                           &type, &format, &count, &remaining, &data) == Success && data) {
        snprintf(c->title, sizeof(c->title), "%s", (const char*)data);
        XFree(data);
    }
    if (!c->title[0]) {
        XTextProperty text;
        if (XGetWMName(d, c->window, &text) && text.value) {
            snprintf(c->title, sizeof(c->title), "%s", (const char*)text.value);
            XFree(text.value);
        }
    }
}

// Keep one NOIR_ICON_SIZE square copy of _NET_WM_ICON for the title bar.
void frame_read_icon(NoirWM* wm, NoirClient* c) {
    c->hasIcon = 0;
    Atom type;
    int format;
    unsigned long count = 0, remaining;
    unsigned char* data = NULL;
    if (XGetWindowProperty(wm->display, c->window, noirAtoms[NET_WM_ICON], 0, 1 << 18, False, XA_CARDINAL,
                           &type, &format, &count, &remaining, &data) != Success || !data) {
        return;
    }
    // The property is a list of images: width, height, then width*height ARGB
    // values. Prefer the smallest one that is at least as large as we draw.
    const unsigned long* values = (const unsigned long*)data;
    const unsigned long* best = NULL;
    unsigned long bestSize = 0;
    for (unsigned long i = 0; format == 32 && i + 2 <= count;) {
        const unsigned long w = values[i], h = values[i + 1];
        if (w == 0 || h == 0 || w > 1024 || h > 1024 || i + 2 + w * h > count) {
            break;
        }
        const int better = !best ||
            (bestSize < NOIR_ICON_SIZE ? w > bestSize : (w >= NOIR_ICON_SIZE && w < bestSize));
        if (better) {
            best = &values[i];
            bestSize = w;
        }
        i += 2 + w * h;
    }
    if (best) {
        const unsigned long w = best[0], h = best[1];
        for (int y = 0; y < NOIR_ICON_SIZE; y++) {
            for (int x = 0; x < NOIR_ICON_SIZE; x++) {
                const unsigned long sx = (unsigned long)x * w / NOIR_ICON_SIZE;
                const unsigned long sy = (unsigned long)y * h / NOIR_ICON_SIZE;
                c->icon[y * NOIR_ICON_SIZE + x] = (unsigned int)best[2 + sy * w + sx];
            }
        }
        c->hasIcon = 1;
    }
    XFree(data);
}

void frame_read_size_hints(NoirWM* wm, NoirClient* c) {
    c->minWidth = c->minHeight = c->maxWidth = c->maxHeight = 0;
    c->baseWidth = c->baseHeight = c->incWidth = c->incHeight = 0;
    XSizeHints hints;
    long supplied;
    if (!XGetWMNormalHints(wm->display, c->window, &hints, &supplied)) {
        return;
    }
    if (hints.flags & PMinSize) { c->minWidth = hints.min_width; c->minHeight = hints.min_height; }
    if (hints.flags & PMaxSize) { c->maxWidth = hints.max_width; c->maxHeight = hints.max_height; }
    if (hints.flags & PBaseSize) { c->baseWidth = hints.base_width; c->baseHeight = hints.base_height; }
    if (hints.flags & PResizeInc) { c->incWidth = hints.width_inc; c->incHeight = hints.height_inc; }
}

static int constrain(int value, int min, int max, int base, int inc, int floor) {
    if (min > floor) floor = min;
    if (value < floor) value = floor;
    if (max > 0 && value > max) value = max;
    if (inc > 1) {
        // Terminals resize in whole character cells.
        const int origin = base > 0 ? base : min;
        value = origin + (value - origin) / inc * inc;
    }
    return value;
}

void frame_constrain_size(const NoirClient* c, int* width, int* height) {
    *width = constrain(*width, c->minWidth, c->maxWidth, c->baseWidth, c->incWidth,
                       MIN_FRAME_WIDTH - 2 * FRAME_BORDER);
    *height = constrain(*height, c->minHeight, c->maxHeight, c->baseHeight, c->incHeight, 1);
}

void frame_apply_geometry(NoirWM* wm, NoirClient* c) {
    Display* d = wm->display;
    XMoveResizeWindow(d, c->frame, c->x, c->y, (unsigned int)frame_width(c), (unsigned int)frame_height(c));
    XMoveResizeWindow(d, c->window, FRAME_BORDER, FRAME_TITLE, (unsigned int)c->width, (unsigned int)c->height);

    // ICCCM 4.1.5: a reparented client only learns its root position from a
    // synthetic ConfigureNotify.
    XEvent event = { 0 };
    event.xconfigure.type = ConfigureNotify;
    event.xconfigure.event = c->window;
    event.xconfigure.window = c->window;
    event.xconfigure.x = c->x + FRAME_BORDER;
    event.xconfigure.y = c->y + FRAME_TITLE;
    event.xconfigure.width = c->width;
    event.xconfigure.height = c->height;
    event.xconfigure.above = None;
    XSendEvent(d, c->window, False, StructureNotifyMask, &event);
}

// --- drawing -----------------------------------------------------------------

static void fill(NoirWM* wm, Window window, GC gc, unsigned long color, int x, int y, int w, int h) {
    XSetForeground(wm->display, gc, color);
    XFillRectangle(wm->display, window, gc, x, y, (unsigned int)w, (unsigned int)h);
}

static void outline(NoirWM* wm, Window window, GC gc, unsigned long color, int x, int y, int w, int h) {
    XSetForeground(wm->display, gc, color);
    XDrawRectangle(wm->display, window, gc, x, y, (unsigned int)(w - 1), (unsigned int)(h - 1));
}

static void draw_button(NoirWM* wm, NoirClient* c, GC gc, FrameHit button, int active) {
    Display* d = wm->display;
    const int x = buttons_left(c) + (button - HIT_MINIMIZE) * FRAME_BUTTON_WIDTH;
    const int hovered = c->hoverButton == (int)button;
    const int pressed = hovered && c->pressedButton == (int)button;
    unsigned long glyph = active ? COLOR_SILVER : COLOR_SILVER_DIM;
    if (hovered) {
        // VDD: interactive elements invert under the pointer.
        fill(wm, c->frame, gc, pressed ? COLOR_SILVER : COLOR_WHITE, x, 1, FRAME_BUTTON_WIDTH, FRAME_TITLE - 2);
        glyph = COLOR_BLACK;
    }
    // Glyphs live in a 10x10 box in the middle of the button; a pressed
    // button sinks by a pixel.
    const int gx = x + (FRAME_BUTTON_WIDTH - 10) / 2, gy = (FRAME_TITLE - 10) / 2 + pressed;
    XSetForeground(d, gc, glyph);
    switch (button) {
    case HIT_MINIMIZE:
        XFillRectangle(d, c->frame, gc, gx, gy + 8, 10, 2);
        break;
    case HIT_MAXIMIZE:
        if (c->maximized) {
            // Restore: two overlapping squares.
            XDrawRectangle(d, c->frame, gc, gx + 3, gy, 6, 6);
            XFillRectangle(d, c->frame, gc, gx + 3, gy, 7, 2);
            fill(wm, c->frame, gc, hovered ? (pressed ? COLOR_SILVER : COLOR_WHITE)
                                           : (active ? COLOR_PANEL : COLOR_PANEL_DIM), gx, gy + 3, 7, 7);
            XSetForeground(d, gc, glyph);
            XDrawRectangle(d, c->frame, gc, gx, gy + 3, 6, 6);
            XFillRectangle(d, c->frame, gc, gx, gy + 3, 7, 2);
        } else {
            XDrawRectangle(d, c->frame, gc, gx, gy, 9, 9);
            XFillRectangle(d, c->frame, gc, gx, gy, 10, 2);
        }
        break;
    case HIT_CLOSE:
        for (int i = 0; i < 2; i++) {
            XDrawLine(d, c->frame, gc, gx + i, gy, gx + 9, gy + 9 - i);
            XDrawLine(d, c->frame, gc, gx, gy + i, gx + 9 - i, gy + 9);
            XDrawLine(d, c->frame, gc, gx + 9 - i, gy, gx, gy + 9 - i);
            XDrawLine(d, c->frame, gc, gx + 9, gy + i, gx + i, gy + 9);
        }
        break;
    default:
        break;
    }
}

static void draw_icon(NoirWM* wm, NoirClient* c, GC gc, unsigned long background, int x, int y) {
    Display* d = wm->display;
    const int screen = DefaultScreen(d);
    if (DefaultDepth(d, screen) != 24) {
        return;
    }
    // Blend against the title bar here; the frame itself has no alpha.
    unsigned int* pixels = malloc(sizeof(unsigned int) * NOIR_ICON_SIZE * NOIR_ICON_SIZE);
    for (int i = 0; i < NOIR_ICON_SIZE * NOIR_ICON_SIZE; i++) {
        const unsigned int argb = c->icon[i], a = argb >> 24;
        unsigned int out = 0;
        for (int shift = 0; shift < 24; shift += 8) {
            const unsigned int fg = (argb >> shift) & 0xff, bg = (unsigned int)(background >> shift) & 0xff;
            out |= ((fg * a + bg * (255 - a)) / 255) << shift;
        }
        pixels[i] = out;
    }
    XImage* image = XCreateImage(d, DefaultVisual(d, screen), 24, ZPixmap, 0, (char*)pixels,
                                 NOIR_ICON_SIZE, NOIR_ICON_SIZE, 32, 0);
    if (image) {
        XPutImage(d, c->frame, gc, image, 0, 0, x, y, NOIR_ICON_SIZE, NOIR_ICON_SIZE);
        XDestroyImage(image);   // frees `pixels` too
    } else {
        free(pixels);
    }
}

void frame_draw(NoirWM* wm, NoirClient* c) {
    if (!c->frame) {
        return;
    }
    Display* d = wm->display;
    const int active = c->window == wm->activeWindow;
    const int fw = frame_width(c), fh = frame_height(c);
    const unsigned long panel = active ? COLOR_PANEL : COLOR_PANEL_DIM;
    GC gc = XCreateGC(d, c->frame, 0, NULL);

    // Children clip their parent, so this only paints the decoration.
    fill(wm, c->frame, gc, panel, 0, 0, fw, fh);
    fill(wm, c->frame, gc, COLOR_SILVER_FAINT, 1, FRAME_TITLE - 1, fw - 2, 1);
    outline(wm, c->frame, gc, active ? COLOR_SILVER : COLOR_SILVER_FAINT, 0, 0, fw, fh);

    // Left: icon, then the title, clipped short of the buttons.
    int textX = 10;
    if (c->hasIcon) {
        draw_icon(wm, c, gc, panel, 8, (FRAME_TITLE - NOIR_ICON_SIZE) / 2);
        textX = 8 + NOIR_ICON_SIZE + 8;
    }
    if (wm->font && c->draw && c->title[0]) {
        const unsigned long text = active ? COLOR_WHITE : COLOR_SILVER_DIM;
        XRenderColor renderColor = { (unsigned short)(((text >> 16) & 0xff) * 257),
                                     (unsigned short)(((text >> 8) & 0xff) * 257),
                                     (unsigned short)((text & 0xff) * 257), 0xffff };
        const int screen = DefaultScreen(d);
        XftColor color;
        if (XftColorAllocValue(d, DefaultVisual(d, screen), DefaultColormap(d, screen), &renderColor, &color)) {
            XRectangle clip = { (short)textX, 1, (unsigned short)(buttons_left(c) - textX - 8 > 0
                                                                 ? buttons_left(c) - textX - 8 : 0),
                                FRAME_TITLE - 2 };
            XftDrawSetClipRectangles(c->draw, 0, 0, &clip, 1);
            const int baseline = (FRAME_TITLE + wm->font->ascent - wm->font->descent) / 2;
            XftDrawStringUtf8(c->draw, &color, wm->font, textX, baseline,
                              (const FcChar8*)c->title, (int)strlen(c->title));
            XftColorFree(d, DefaultVisual(d, screen), DefaultColormap(d, screen), &color);
        }
    }

    // Right: minimize, maximize/restore, close.
    draw_button(wm, c, gc, HIT_MINIMIZE, active);
    draw_button(wm, c, gc, HIT_MAXIMIZE, active);
    draw_button(wm, c, gc, HIT_CLOSE, active);
    XFreeGC(d, gc);
}

// --- interaction -------------------------------------------------------------

static void set_hover(NoirWM* wm, NoirClient* c, int button) {
    if (c->hoverButton != button) {
        c->hoverButton = button;
        frame_draw(wm, c);
    }
}

void frame_handle_button_press(NoirWM* wm, NoirClient* c, XButtonEvent* event) {
    const Window window = c->window;
    if (window != wm->activeWindow) {
        client_activate(wm, c);
        c = client_find(wm, window);
        if (!c) {
            return;
        }
    }
    if (event->button != Button1) {
        return;
    }
    int edges;
    const FrameHit hit = hit_test(c, event->x, event->y, &edges);
    switch (hit) {
    case HIT_MINIMIZE:
    case HIT_MAXIMIZE:
    case HIT_CLOSE:
        // Acts on release, if the pointer is still over the button.
        c->pressedButton = hit;
        c->hoverButton = hit;
        frame_draw(wm, c);
        break;
    case HIT_TITLE:
        if (wm->lastClickClient == window && event->time - wm->lastClickTime < DOUBLE_CLICK_MS) {
            wm->lastClickClient = None;
            client_set_maximized(wm, c, !c->maximized);
            break;
        }
        wm->lastClickClient = window;
        wm->lastClickTime = event->time;
        if (c->maximized) {
            break;
        }
        /* fall through */
    case HIT_RESIZE:
        wm->dragMode = hit == HIT_TITLE ? DRAG_MOVE : DRAG_RESIZE;
        wm->dragClient = window;
        wm->dragEdges = edges;
        wm->dragStartX = event->x_root;
        wm->dragStartY = event->y_root;
        wm->dragX = c->x; wm->dragY = c->y;
        wm->dragWidth = c->width; wm->dragHeight = c->height;
        break;
    default:
        break;
    }
}

void frame_handle_button_release(NoirWM* wm, XButtonEvent* event) {
    if (event->button != Button1) {
        return;
    }
    wm->dragMode = DRAG_NONE;
    NoirClient* c = client_find_frame(wm, event->window);
    if (!c || !c->pressedButton) {
        return;
    }
    int edges;
    const int pressed = c->pressedButton;
    const int confirmed = (int)hit_test(c, event->x, event->y, &edges) == pressed;
    c->pressedButton = 0;
    c->hoverButton = confirmed ? pressed : 0;
    frame_draw(wm, c);
    if (!confirmed) {
        return;
    }
    // Each of these may move or drop entries in wm->clients: `c` is dead after.
    if (pressed == HIT_MINIMIZE) {
        c->hoverButton = 0;
        client_minimize(wm, c);
    } else if (pressed == HIT_MAXIMIZE) {
        client_set_maximized(wm, c, !c->maximized);
    } else {
        client_close(wm, c);
    }
}

static void drag_update(NoirWM* wm, NoirClient* c, int rootX, int rootY) {
    const int dx = rootX - wm->dragStartX, dy = rootY - wm->dragStartY;
    long area[4];
    wm_get_workarea(wm, area);
    if (wm->dragMode == DRAG_MOVE) {
        c->x = wm->dragX + dx;
        c->y = wm->dragY + dy;
    } else {
        int width = wm->dragWidth, height = wm->dragHeight;
        if (wm->dragEdges & FRAME_EDGE_LEFT) width -= dx;
        if (wm->dragEdges & FRAME_EDGE_RIGHT) width += dx;
        if (wm->dragEdges & FRAME_EDGE_TOP) height -= dy;
        if (wm->dragEdges & FRAME_EDGE_BOTTOM) height += dy;
        frame_constrain_size(c, &width, &height);
        // Dragging the left or top edge keeps the opposite edge in place.
        if (wm->dragEdges & FRAME_EDGE_LEFT) c->x = wm->dragX + wm->dragWidth - width;
        if (wm->dragEdges & FRAME_EDGE_TOP) c->y = wm->dragY + wm->dragHeight - height;
        c->width = width;
        c->height = height;
    }
    // Keep the title bar reachable: never above or below the work area.
    const int maxY = (int)(area[1] + area[3]) - FRAME_TITLE;
    if (c->y > maxY) c->y = maxY;
    if (c->y < area[1]) c->y = (int)area[1];
    frame_apply_geometry(wm, c);
    if (wm->dragMode == DRAG_RESIZE) {
        frame_draw(wm, c);
    }
}

void frame_handle_motion(NoirWM* wm, XMotionEvent* event) {
    // Only the most recent pointer position matters.
    XEvent next;
    while (XCheckTypedWindowEvent(wm->display, event->window, MotionNotify, &next)) {
        *event = next.xmotion;
    }
    if (wm->dragMode != DRAG_NONE) {
        NoirClient* c = client_find(wm, wm->dragClient);
        if (c && c->frame) {
            drag_update(wm, c, event->x_root, event->y_root);
        } else {
            wm->dragMode = DRAG_NONE;
        }
        return;
    }
    NoirClient* c = client_find_frame(wm, event->window);
    if (!c) {
        return;
    }
    int edges;
    const FrameHit hit = hit_test(c, event->x, event->y, &edges);
    const int overButton = hit >= HIT_MINIMIZE && hit <= HIT_CLOSE;
    set_hover(wm, c, overButton ? (int)hit : 0);
    XDefineCursor(wm->display, c->frame, wm->cursors[hit == HIT_RESIZE ? edges : 0]);
}

void frame_handle_leave(NoirWM* wm, NoirClient* c) {
    // While a button is held the implicit grab keeps events coming.
    if (!c->pressedButton) {
        set_hover(wm, c, 0);
    }
}
