// noir-xinject: synthesize pointer and keyboard input on an X display through
// the XTEST extension. Used by the Xvfb harness (tools/xvfb/noir-xvfb) to
// drive the shell where no real input devices exist.
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define STEP_DELAY_US 20000

static void usage(const char* self) {
    fprintf(stderr,
        "Usage: %s <command> [args]\n"
        "  move X Y              move the pointer\n"
        "  click X Y [BUTTON]    move, then press and release BUTTON (default 1)\n"
        "  down|up [BUTTON]      press or release a button at the current position\n"
        "  drag X1 Y1 X2 Y2      drag with button 1\n"
        "  scroll AMOUNT         wheel up (positive) or down (negative)\n"
        "  key COMBO...          press key combinations, e.g. alt+F1 super Return\n"
        "  type TEXT             type Latin-1 text\n", self);
}

static void settle(Display* dpy) {
    XFlush(dpy);
    usleep(STEP_DELAY_US);
}

static void move(Display* dpy, int x, int y) {
    XTestFakeMotionEvent(dpy, DefaultScreen(dpy), x, y, CurrentTime);
    settle(dpy);
}

static void button(Display* dpy, unsigned int number, Bool press) {
    XTestFakeButtonEvent(dpy, number, press, CurrentTime);
    settle(dpy);
}

static KeySym lookupKey(const char* name) {
    static const struct { const char* alias; KeySym keysym; } aliases[] = {
        { "ctrl", XK_Control_L }, { "control", XK_Control_L }, { "shift", XK_Shift_L },
        { "alt", XK_Alt_L }, { "super", XK_Super_L }, { "meta", XK_Super_L },
        { "win", XK_Super_L }, { "enter", XK_Return }, { "esc", XK_Escape },
        { "del", XK_Delete }, { "space", XK_space }, { "menu", XK_Menu },
        { "pgup", XK_Page_Up }, { "pgdn", XK_Page_Down },
    };
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (strcasecmp(name, aliases[i].alias) == 0) return aliases[i].keysym;
    }
    return XStringToKeysym(name);
}

// Press every key of "mod+mod+key" in order, then release in reverse.
static int keyCombo(Display* dpy, const char* combo) {
    KeyCode codes[8];
    int count = 0;
    char* copy = strdup(combo);
    for (char* part = strtok(copy, "+"); part; part = strtok(NULL, "+")) {
        KeySym keysym = lookupKey(part);
        KeyCode code = keysym == NoSymbol ? 0 : XKeysymToKeycode(dpy, keysym);
        if (!code || count == 8) {
            fprintf(stderr, "noir-xinject: unknown key \"%s\" in \"%s\"\n", part, combo);
            free(copy);
            return 0;
        }
        codes[count++] = code;
    }
    free(copy);
    for (int i = 0; i < count; i++) { XTestFakeKeyEvent(dpy, codes[i], True, CurrentTime); settle(dpy); }
    for (int i = count - 1; i >= 0; i--) { XTestFakeKeyEvent(dpy, codes[i], False, CurrentTime); settle(dpy); }
    return 1;
}

static int typeText(Display* dpy, const char* text) {
    KeyCode shift = XKeysymToKeycode(dpy, XK_Shift_L);
    for (const unsigned char* c = (const unsigned char*)text; *c; c++) {
        KeySym keysym = *c == '\n' ? XK_Return : *c == '\t' ? XK_Tab : (KeySym)*c;
        KeyCode code = XKeysymToKeycode(dpy, keysym);
        if (!code) {
            fprintf(stderr, "noir-xinject: no key for character 0x%02x\n", *c);
            return 0;
        }
        Bool shifted = XkbKeycodeToKeysym(dpy, code, 0, 0) != keysym
                    && XkbKeycodeToKeysym(dpy, code, 0, 1) == keysym;
        if (shifted) XTestFakeKeyEvent(dpy, shift, True, CurrentTime);
        XTestFakeKeyEvent(dpy, code, True, CurrentTime);
        XTestFakeKeyEvent(dpy, code, False, CurrentTime);
        if (shifted) XTestFakeKeyEvent(dpy, shift, False, CurrentTime);
        settle(dpy);
    }
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(argv[0]);
        return argc < 2 ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "noir-xinject: cannot open display\n");
        return EXIT_FAILURE;
    }
    int event, error, major, minor;
    if (!XTestQueryExtension(dpy, &event, &error, &major, &minor)) {
        fprintf(stderr, "noir-xinject: the X server has no XTEST extension\n");
        return EXIT_FAILURE;
    }

    const char* command = argv[1];
    int nargs = argc - 2;
    char** args = argv + 2;
    int ok = 1;

    if (strcmp(command, "move") == 0 && nargs == 2) {
        move(dpy, atoi(args[0]), atoi(args[1]));
    } else if (strcmp(command, "click") == 0 && (nargs == 2 || nargs == 3)) {
        unsigned int number = nargs == 3 ? (unsigned int)atoi(args[2]) : 1;
        move(dpy, atoi(args[0]), atoi(args[1]));
        button(dpy, number, True);
        button(dpy, number, False);
    } else if ((strcmp(command, "down") == 0 || strcmp(command, "up") == 0) && nargs <= 1) {
        button(dpy, nargs == 1 ? (unsigned int)atoi(args[0]) : 1, command[0] == 'd');
    } else if (strcmp(command, "drag") == 0 && nargs == 4) {
        int x1 = atoi(args[0]), y1 = atoi(args[1]), x2 = atoi(args[2]), y2 = atoi(args[3]);
        move(dpy, x1, y1);
        button(dpy, 1, True);
        for (int i = 1; i <= 10; i++) move(dpy, x1 + (x2 - x1) * i / 10, y1 + (y2 - y1) * i / 10);
        button(dpy, 1, False);
    } else if (strcmp(command, "scroll") == 0 && nargs == 1) {
        int amount = atoi(args[0]);
        for (int i = 0; i < abs(amount); i++) {
            button(dpy, amount > 0 ? 4 : 5, True);
            button(dpy, amount > 0 ? 4 : 5, False);
        }
    } else if (strcmp(command, "key") == 0 && nargs >= 1) {
        for (int i = 0; i < nargs && ok; i++) ok = keyCombo(dpy, args[i]);
    } else if (strcmp(command, "type") == 0 && nargs == 1) {
        ok = typeText(dpy, args[0]);
    } else {
        usage(argv[0]);
        ok = 0;
    }

    XSync(dpy, False);
    XCloseDisplay(dpy);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
