#include "Compositor.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static Display* display;
static Window root, overlay;
static NoirCompositor* compositor;
static int errors, failures;

static long long milliseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int xerror(Display* d, XErrorEvent* event) {
    char message[256];
    XGetErrorText(d, event->error_code, message, sizeof(message));
    fprintf(stderr, "X11: %s (request %d, resource 0x%lx)\n", message, event->request_code, event->resourceid);
    errors++;
    return 0;
}

static void pump(int duration) {
    const long long end = milliseconds() + duration;
    do {
        XSync(display, False);
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            compositor_handle_event(compositor, &event);
        }
        if (compositor_needs_frame(compositor) && !compositor_frame_delay(compositor))
            compositor_draw(compositor);
        usleep(2000);
    } while (milliseconds() < end);
}

static void check(int condition, const char* name) {
    printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) failures++;
}

typedef struct {
    int count, outside, minX, minY, maxX, maxY;
} Pixels;

static Pixels pixels(int x, int y, int width, int height) {
    Pixels result = { .minX = 640, .minY = 480 };
    XImage* image = XGetImage(display, overlay, 0, 0, 640, 480, AllPlanes, ZPixmap);
    if (!image) {
        fprintf(stderr, "Cannot read compositor output\n");
        exit(1);
    }
    for (int py = 0; py < 480; py++) {
        for (int px = 0; px < 640; px++) {
            unsigned long p = XGetPixel(image, px, py);
            if ((p & image->red_mask) < (image->red_mask * 3 / 4) ||
                (p & image->green_mask) > image->green_mask / 2 ||
                (p & image->blue_mask) > image->blue_mask / 2)
                continue;
            result.count++;
            result.outside += px < x || py < y || px >= x + width || py >= y + height;
            if (px < result.minX) result.minX = px;
            if (py < result.minY) result.minY = py;
            if (px > result.maxX) result.maxX = px;
            if (py > result.maxY) result.maxY = py;
        }
    }
    XDestroyImage(image);
    return result;
}

static void settings(int reduced, float scale) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/SiriusOS/Noir.ini", getenv("XDG_CONFIG_HOME"));
    FILE* file = fopen(path, "w");
    if (!file) { perror(path); exit(1); }
    fprintf(file, "[Appearance]\nReducedMotion=%s\nAnimationScale=%.2f\n", reduced ? "true" : "false", scale);
    if (fclose(file)) { perror(path); exit(1); }
    pump(300);
}

static Window make_window(int frameless) {
    Window window = XCreateSimpleWindow(display, root, 80, 80, 160, 120, 0, 0, 0xf04040);
    if (!frameless) compositor_set_shadow(compositor, window, 1);
    const unsigned long target[] = { 430, 430, 120, 32 };
    XChangeProperty(display, window, XInternAtom(display, "_NET_WM_ICON_GEOMETRY", False),
                    XA_CARDINAL, 32, PropModeReplace, (const unsigned char*)target, 4);
    XMapWindow(display, window);
    pump(250);
    return window;
}

int main(void) {
    display = XOpenDisplay(NULL);
    if (!display) { fprintf(stderr, "Cannot open test display\n"); return 1; }
    XSetErrorHandler(xerror);
    root = DefaultRootWindow(display);
    XSelectInput(display, root, SubstructureNotifyMask);
    compositor = compositor_create(display, root);
    if (!compositor) return 1;
    overlay = XCompositeGetOverlayWindow(display, root);
    Window window = make_window(0);
    Pixels p = pixels(80, 80, 160, 120);
    check(p.count == 160 * 120 && !p.outside, "stationary windows remain pixel exact");

    compositor_move(compositor, window, 100, 40, 20, 10);
    XMoveWindow(display, window, 180, 120);
    pump(70);
    p = pixels(180, 120, 160, 120);
    check(p.count > 1000 && p.outside > 100, "moving windows deform beyond their rigid rectangle");
    pump(1800);
    p = pixels(180, 120, 160, 120);
    check(p.count == 160 * 120 && !p.outside, "wobble settles to the exact final geometry");
    check(!compositor_needs_frame(compositor), "settled effects stop requesting frames");

    compositor_minimize(compositor, window, window, 1);
    XUnmapWindow(display, window);
    pump(230);
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);
    p = pixels(180, 120, 160, 120);
    if (!(attrs.map_state == IsUnmapped && p.count > 100 && p.maxY > 244 && p.minX > 180))
        fprintf(stderr, "minimize: map=%d pixels=%d bounds=%d,%d..%d,%d\n",
                attrs.map_state, p.count, p.minX, p.minY, p.maxX, p.maxY);
    check(attrs.map_state == IsUnmapped && p.count > 100 && p.maxY > 244 && p.minX > 180,
          "minimize unmaps immediately while its mesh curves toward the task tile");
    pump(500);
    check(!pixels(0, 0, 640, 480).count, "minimize leaves no stale pixels");
    check(!compositor_needs_frame(compositor), "completed minimize stops requesting frames");

    compositor_minimize(compositor, window, window, 0);
    XMapWindow(display, window);
    pump(200);
    p = pixels(180, 120, 160, 120);
    check(p.count > 100 && p.maxY > 260, "restore expands back out of the task tile");
    pump(500);
    p = pixels(180, 120, 160, 120);
    check(p.count == 160 * 120 && !p.outside, "restore ends pixel exact");

    compositor_minimize(compositor, window, window, 1);
    XUnmapWindow(display, window);
    pump(220);
    Pixels before = pixels(180, 120, 160, 120);
    compositor_minimize(compositor, window, window, 0);
    XMapWindow(display, window);
    pump(20);
    p = pixels(180, 120, 160, 120);
    check(p.count > 100 && abs(p.minX - before.minX) < 40 && abs(p.maxY - before.maxY) < 40,
          "reversing a lamp transition preserves its current shape");
    pump(500);
    check(pixels(180, 120, 160, 120).count == 160 * 120, "reversed restore completes");

    compositor_close(compositor, window);
    XDestroyWindow(display, window);
    pump(230);
    p = pixels(180, 120, 160, 120);
    check(p.count > 100 && p.outside > 100, "shards remain visible after their X window is destroyed");
    pump(650);
    check(!pixels(0, 0, 640, 480).count && !compositor_needs_frame(compositor),
          "shatter releases its snapshot and stops drawing");

    window = make_window(1);
    settings(0, 3.0f);
    compositor_minimize(compositor, window, window, 1);
    XUnmapWindow(display, window);
    pump(750);
    check(pixels(0, 0, 640, 480).count > 100, "animation scale extends the lamp duration");
    pump(1100);
    check(!pixels(0, 0, 640, 480).count, "scaled lamp still completes");
    compositor_minimize(compositor, window, window, 0);
    XMapWindow(display, window);
    pump(1900);
    settings(1, 1.0f);
    compositor_move(compositor, window, 100, 40, 20, 10);
    XMoveWindow(display, window, 180, 120);
    pump(60);
    p = pixels(180, 120, 160, 120);
    check(p.count == 160 * 120 && !p.outside, "Reduce Motion disables wobble");
    compositor_minimize(compositor, window, window, 1);
    XUnmapWindow(display, window);
    pump(60);
    check(!pixels(0, 0, 640, 480).count, "Reduce Motion makes minimize immediate");
    compositor_minimize(compositor, window, window, 0);
    XMapWindow(display, window);
    pump(80);
    check(pixels(180, 120, 160, 120).count == 160 * 120, "Reduce Motion makes restore immediate");
    compositor_close(compositor, window);
    XDestroyWindow(display, window);
    pump(60);
    check(!pixels(0, 0, 640, 480).count, "Reduce Motion makes close immediate");

    settings(0, 1.0f);
    window = make_window(1);
    for (int i = 0; i < 8; i++) {
        compositor_minimize(compositor, window, window, 1);
        XUnmapWindow(display, window);
        pump(45);
        compositor_minimize(compositor, window, window, 0);
        XMapWindow(display, window);
        pump(35);
    }
    pump(700);
    check(pixels(80, 80, 160, 120).count == 160 * 120, "rapid toggles do not lose window contents");
    compositor_minimize(compositor, window, window, 1);
    XUnmapWindow(display, window);
    pump(100);
    settings(1, 1.0f);
    pump(80);
    check(!pixels(0, 0, 640, 480).count, "enabling Reduce Motion finishes an in-flight animation");
    XDestroyWindow(display, window);
    pump(50);
    compositor_destroy(compositor);
    XCompositeReleaseOverlayWindow(display, root);
    XSync(display, False);
    check(!errors, "window lifecycle produces no X11 errors");
    XCloseDisplay(display);
    return failures ? 1 : 0;
}
