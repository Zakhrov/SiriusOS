#include "Compositor.h"
#include "VulkanRenderer.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#define SHADOW_RADIUS 22
#define SHADOW_OFFSET_Y 6
#define SHADOW_STRENGTH 0.55f
#define FRAME_INTERVAL_MS 16

// One top-level window (a direct child of the root).
typedef struct {
    Window id;
    int x, y, width, height, border;
    int depth;
    Visual* visual;
    int viewable;
    int shadow;
    Damage damage;
    Pixmap pixmap;              // off-screen contents, named while viewable
    RendererTexture* texture;
    int stale;                  // whole texture must be refetched
    int dirty;                  // dirty rectangle below is valid
    int dirtyX0, dirtyY0, dirtyX1, dirtyY1;
} CompWindow;

struct NoirCompositor {
    Display* display;
    Window root;
    Window overlay;
    Window selectionOwner;
    int damageEvent;
    int hasShm;
    XShmSegmentInfo shm;
    size_t shmSize;
    VulkanRenderer* renderer;
    CompWindow* windows;        // bottom to top
    int count, capacity;
    int needsFrame;
    long long lastFrameMs;
};

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// --- window list -------------------------------------------------------------

static int find_index(const NoirCompositor* c, Window id) {
    for (int i = 0; i < c->count; i++) {
        if (c->windows[i].id == id) {
            return i;
        }
    }
    return -1;
}

static int outer_width(const CompWindow* w) { return w->width + 2 * w->border; }
static int outer_height(const CompWindow* w) { return w->height + 2 * w->border; }

static void release_contents(NoirCompositor* c, CompWindow* w) {
    if (w->pixmap) {
        XFreePixmap(c->display, w->pixmap);
        w->pixmap = None;
    }
    if (w->texture) {
        renderer_texture_destroy(c->renderer, w->texture);
        w->texture = NULL;
    }
    w->stale = 1;
    w->dirty = 0;
}

// Track `id` (a child of the root) at the top of the stack.
static CompWindow* add_window(NoirCompositor* c, Window id) {
    int index = find_index(c, id);
    if (index >= 0) {
        return &c->windows[index];
    }
    XWindowAttributes wa;
    if (id == c->overlay || !XGetWindowAttributes(c->display, id, &wa) || wa.class == InputOnly) {
        return NULL;
    }
    if (c->count == c->capacity) {
        c->capacity = c->capacity ? c->capacity * 2 : 32;
        c->windows = realloc(c->windows, sizeof(CompWindow) * (size_t)c->capacity);
    }
    CompWindow* w = &c->windows[c->count++];
    memset(w, 0, sizeof(*w));
    w->id = id;
    w->x = wa.x; w->y = wa.y;
    w->width = wa.width; w->height = wa.height;
    w->border = wa.border_width;
    w->depth = wa.depth;
    w->visual = wa.visual;
    w->viewable = wa.map_state == IsViewable;
    w->stale = 1;
    w->damage = XDamageCreate(c->display, id, XDamageReportNonEmpty);
    c->needsFrame = 1;
    return w;
}

static void remove_window(NoirCompositor* c, Window id, int destroyed) {
    int index = find_index(c, id);
    if (index < 0) {
        return;
    }
    CompWindow* w = &c->windows[index];
    release_contents(c, w);
    // The server frees a window's damage objects along with the window.
    if (w->damage && !destroyed) {
        XDamageDestroy(c->display, w->damage);
    }
    memmove(w, w + 1, sizeof(CompWindow) * (size_t)(c->count - index - 1));
    c->count--;
    c->needsFrame = 1;
}

// Move a window to just above `sibling` (None: to the bottom).
static void restack(NoirCompositor* c, Window id, Window sibling) {
    int index = find_index(c, id);
    if (index < 0) {
        return;
    }
    CompWindow moved = c->windows[index];
    memmove(&c->windows[index], &c->windows[index + 1], sizeof(CompWindow) * (size_t)(c->count - index - 1));
    c->count--;
    int target = sibling == None ? 0 : find_index(c, sibling) + 1;   // unknown sibling: bottom
    memmove(&c->windows[target + 1], &c->windows[target], sizeof(CompWindow) * (size_t)(c->count - target));
    c->windows[target] = moved;
    c->count++;
    if (target != index) {
        c->needsFrame = 1;
    }
}

// --- window contents ---------------------------------------------------------

static int ensure_shm(NoirCompositor* c, size_t bytes) {
    if (!c->hasShm) {
        return 0;
    }
    if (bytes <= c->shmSize) {
        return 1;
    }
    if (c->shmSize) {
        XShmDetach(c->display, &c->shm);
        shmdt(c->shm.shmaddr);
        c->shmSize = 0;
    }
    c->shm.shmid = shmget(IPC_PRIVATE, bytes, IPC_CREAT | 0600);
    if (c->shm.shmid < 0) {
        c->hasShm = 0;
        return 0;
    }
    c->shm.shmaddr = shmat(c->shm.shmid, NULL, 0);
    c->shm.readOnly = False;
    // Mark for removal now; the segment lives until both sides detach.
    shmctl(c->shm.shmid, IPC_RMID, NULL);
    if (c->shm.shmaddr == (char*)-1 || !XShmAttach(c->display, &c->shm)) {
        c->hasShm = 0;
        return 0;
    }
    XSync(c->display, False);
    c->shmSize = bytes;
    return 1;
}

// Copy a rectangle of the window's off-screen pixmap into its texture.
static void fetch_rect(NoirCompositor* c, CompWindow* w, int x, int y, int width, int height) {
    uint8_t* pixels = renderer_texture_pixels(w->texture);
    const int stride = outer_width(w) * 4;
    XImage* image = NULL;
    int shared = 0;
    if (ensure_shm(c, (size_t)width * (size_t)height * 4)) {
        image = XShmCreateImage(c->display, w->visual, (unsigned int)w->depth, ZPixmap,
                                c->shm.shmaddr, &c->shm, (unsigned int)width, (unsigned int)height);
        shared = image && XShmGetImage(c->display, w->pixmap, image, x, y, AllPlanes);
        if (image && !shared) {
            XFree(image);
            image = NULL;
        }
    }
    if (!image) {
        image = XGetImage(c->display, w->pixmap, x, y, (unsigned int)width, (unsigned int)height,
                          AllPlanes, ZPixmap);
    }
    if (!image) {
        return;   // the window changed under us; a later event repaints it
    }
    if (image->bits_per_pixel == 32) {
        for (int row = 0; row < height; row++) {
            memcpy(pixels + (size_t)(y + row) * (size_t)stride + (size_t)x * 4,
                   image->data + (size_t)row * (size_t)image->bytes_per_line, (size_t)width * 4);
        }
        renderer_texture_update(c->renderer, w->texture, x, y, width, height);
    }
    if (shared) {
        XFree(image);
    } else {
        XDestroyImage(image);
    }
}

// Bring a viewable window's texture up to date with its damage.
static void refresh_window(NoirCompositor* c, CompWindow* w) {
    const int width = outer_width(w), height = outer_height(w);
    if (!w->pixmap) {
        w->pixmap = XCompositeNameWindowPixmap(c->display, w->id);
        w->stale = 1;
    }
    if (!w->texture) {
        w->texture = renderer_texture_create(c->renderer, width, height);
        w->stale = 1;
    }
    if (!w->pixmap || !w->texture) {
        return;
    }
    if (w->stale) {
        fetch_rect(c, w, 0, 0, width, height);
    } else if (w->dirty) {
        int x0 = w->dirtyX0 < 0 ? 0 : w->dirtyX0, y0 = w->dirtyY0 < 0 ? 0 : w->dirtyY0;
        int x1 = w->dirtyX1 > width ? width : w->dirtyX1, y1 = w->dirtyY1 > height ? height : w->dirtyY1;
        if (x1 > x0 && y1 > y0) {
            fetch_rect(c, w, x0, y0, x1 - x0, y1 - y0);
        }
    }
    w->stale = 0;
    w->dirty = 0;
}

static void handle_damage(NoirCompositor* c, XDamageNotifyEvent* event) {
    int index = find_index(c, event->drawable);
    if (index < 0) {
        return;
    }
    CompWindow* w = &c->windows[index];
    // Collect the damaged region (relative to the window's origin, inside its
    // border) and reset the damage object so it reports again.
    XserverRegion region = XFixesCreateRegion(c->display, NULL, 0);
    XDamageSubtract(c->display, w->damage, None, region);
    XRectangle bounds = { 0, 0, 0, 0 };
    int rectCount = 0;
    XRectangle* rects = XFixesFetchRegionAndBounds(c->display, region, &rectCount, &bounds);
    if (rects) XFree(rects);
    XFixesDestroyRegion(c->display, region);
    if (rectCount == 0) {
        return;
    }
    const int x0 = bounds.x + w->border, y0 = bounds.y + w->border;
    const int x1 = x0 + bounds.width, y1 = y0 + bounds.height;
    if (!w->dirty) {
        w->dirty = 1;
        w->dirtyX0 = x0; w->dirtyY0 = y0; w->dirtyX1 = x1; w->dirtyY1 = y1;
    } else {
        if (x0 < w->dirtyX0) w->dirtyX0 = x0;
        if (y0 < w->dirtyY0) w->dirtyY0 = y0;
        if (x1 > w->dirtyX1) w->dirtyX1 = x1;
        if (y1 > w->dirtyY1) w->dirtyY1 = y1;
    }
    if (w->viewable) {
        c->needsFrame = 1;
    }
}

// --- events ------------------------------------------------------------------

void compositor_handle_event(NoirCompositor* c, XEvent* event) {
    if (event->type == c->damageEvent + XDamageNotify) {
        handle_damage(c, (XDamageNotifyEvent*)event);
        return;
    }
    switch (event->type) {
    case CreateNotify:
        if (event->xcreatewindow.parent == c->root) {
            add_window(c, event->xcreatewindow.window);
        }
        break;
    case DestroyNotify:
        if (event->xdestroywindow.event == c->root) {
            remove_window(c, event->xdestroywindow.window, 1);
        }
        break;
    case ReparentNotify:
        if (event->xreparent.parent == c->root) {
            add_window(c, event->xreparent.window);
        } else if (event->xreparent.event == c->root) {
            remove_window(c, event->xreparent.window, 0);
        }
        break;
    case MapNotify: {
        int index = event->xmap.event == c->root ? find_index(c, event->xmap.window) : -1;
        if (index >= 0) {
            // Mapping gives the window a new off-screen pixmap.
            release_contents(c, &c->windows[index]);
            c->windows[index].viewable = 1;
            c->needsFrame = 1;
        }
        break;
    }
    case UnmapNotify: {
        int index = event->xunmap.event == c->root ? find_index(c, event->xunmap.window) : -1;
        if (index >= 0) {
            release_contents(c, &c->windows[index]);
            c->windows[index].viewable = 0;
            c->needsFrame = 1;
        }
        break;
    }
    case ConfigureNotify: {
        XConfigureEvent* e = &event->xconfigure;
        int index = e->event == c->root ? find_index(c, e->window) : -1;
        if (index < 0) {
            break;
        }
        CompWindow* w = &c->windows[index];
        if (w->width != e->width || w->height != e->height || w->border != e->border_width) {
            // Resizing replaces the off-screen pixmap as well.
            release_contents(c, w);
            w->width = e->width; w->height = e->height;
            w->border = e->border_width;
            c->needsFrame = 1;
        }
        if (w->x != e->x || w->y != e->y) {
            w->x = e->x; w->y = e->y;
            c->needsFrame = 1;
        }
        restack(c, e->window, e->above);
        break;
    }
    case CirculateNotify:
        if (event->xcirculate.event == c->root && c->count > 0) {
            restack(c, event->xcirculate.window,
                    event->xcirculate.place == PlaceOnTop ? c->windows[c->count - 1].id : None);
        }
        break;
    default:
        break;
    }
}

void compositor_set_shadow(NoirCompositor* c, Window toplevel, int enabled) {
    // The window may have been created a moment ago, before its CreateNotify.
    CompWindow* w = add_window(c, toplevel);
    if (w && w->shadow != enabled) {
        w->shadow = enabled;
        c->needsFrame = 1;
    }
}

// --- drawing -----------------------------------------------------------------

int compositor_needs_frame(const NoirCompositor* c) {
    return c->needsFrame;
}

int compositor_frame_delay(const NoirCompositor* c) {
    const long long elapsed = now_ms() - c->lastFrameMs;
    return elapsed >= FRAME_INTERVAL_MS ? 0 : (int)(FRAME_INTERVAL_MS - elapsed);
}

void compositor_draw(NoirCompositor* c) {
    c->needsFrame = 0;
    c->lastFrameMs = now_ms();
    for (int i = 0; i < c->count; i++) {
        CompWindow* w = &c->windows[i];
        if (w->viewable && (w->depth == 24 || w->depth == 32)) {
            refresh_window(c, w);
        }
    }
    if (!renderer_begin_frame(c->renderer)) {
        c->needsFrame = 1;
        return;
    }
    for (int i = 0; i < c->count; i++) {
        CompWindow* w = &c->windows[i];
        if (!w->viewable || !w->texture) {
            continue;
        }
        if (w->shadow) {
            renderer_draw_shadow(c->renderer, w->x, w->y + SHADOW_OFFSET_Y, outer_width(w), outer_height(w),
                                 SHADOW_RADIUS, SHADOW_STRENGTH);
        }
        renderer_draw_texture(c->renderer, w->texture, w->x, w->y, outer_width(w), outer_height(w),
                              w->depth == 32, 1.0f);
    }
    renderer_end_frame(c->renderer);
}

// --- lifetime ----------------------------------------------------------------

const char* compositor_device_name(const NoirCompositor* c) {
    return renderer_device_name(c->renderer);
}

NoirCompositor* compositor_create(Display* display, Window root) {
    int eventBase, errorBase, major = 0, minor = 2;
    if (!XCompositeQueryExtension(display, &eventBase, &errorBase) ||
        !XCompositeQueryVersion(display, &major, &minor) || (major == 0 && minor < 3)) {
        fprintf(stderr, "NoirCompositor: the X server lacks Composite 0.3\n");
        return NULL;
    }
    int damageEvent, fixesEvent;
    if (!XDamageQueryExtension(display, &damageEvent, &errorBase) ||
        !XFixesQueryExtension(display, &fixesEvent, &errorBase)) {
        fprintf(stderr, "NoirCompositor: the X server lacks Damage or XFixes\n");
        return NULL;
    }
    int damageMajor = 1, damageMinor = 1, fixesMajor = 3, fixesMinor = 0;
    XDamageQueryVersion(display, &damageMajor, &damageMinor);
    XFixesQueryVersion(display, &fixesMajor, &fixesMinor);

    NoirCompositor* c = calloc(1, sizeof(NoirCompositor));
    c->display = display;
    c->root = root;
    c->damageEvent = damageEvent;
    c->hasShm = XShmQueryExtension(display);

    // The overlay sits above every window and below the pointer. It must not
    // take input, so clicks fall through to the real windows underneath.
    c->overlay = XCompositeGetOverlayWindow(display, root);
    XserverRegion empty = XFixesCreateRegion(display, NULL, 0);
    XFixesSetWindowShapeRegion(display, c->overlay, ShapeInput, 0, 0, empty);
    XFixesDestroyRegion(display, empty);

    c->renderer = renderer_create(display, c->overlay);
    if (!c->renderer) {
        XCompositeReleaseOverlayWindow(display, root);
        free(c);
        return NULL;
    }

    XGrabServer(display);
    XCompositeRedirectSubwindows(display, root, CompositeRedirectManual);
    Window rootReturn, parentReturn, *children = NULL;
    unsigned int childCount = 0;
    if (XQueryTree(display, root, &rootReturn, &parentReturn, &children, &childCount)) {
        for (unsigned int i = 0; i < childCount; i++) {
            add_window(c, children[i]);
        }
        if (children) XFree(children);
    }
    XUngrabServer(display);

    // EWMH: owning _NET_WM_CM_Sn tells clients that ARGB windows are blended.
    char name[32];
    snprintf(name, sizeof(name), "_NET_WM_CM_S%d", DefaultScreen(display));
    c->selectionOwner = XCreateSimpleWindow(display, root, -1, -1, 1, 1, 0, 0, 0);
    XSetSelectionOwner(display, XInternAtom(display, name, False), c->selectionOwner, CurrentTime);

    c->needsFrame = 1;
    return c;
}

void compositor_destroy(NoirCompositor* c) {
    if (!c) {
        return;
    }
    for (int i = 0; i < c->count; i++) {
        release_contents(c, &c->windows[i]);
        if (c->windows[i].damage) XDamageDestroy(c->display, c->windows[i].damage);
    }
    renderer_destroy(c->renderer);
    if (c->shmSize) {
        XShmDetach(c->display, &c->shm);
        shmdt(c->shm.shmaddr);
    }
    XCompositeUnredirectSubwindows(c->display, c->root, CompositeRedirectManual);
    XCompositeReleaseOverlayWindow(c->display, c->root);
    if (c->selectionOwner) XDestroyWindow(c->display, c->selectionOwner);
    free(c->windows);
    free(c);
}
