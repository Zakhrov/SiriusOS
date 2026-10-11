#include "Ewmh.h"

#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>

Atom noirAtoms[NOIR_ATOM_COUNT];

static char* atomNames[NOIR_ATOM_COUNT] = {
    [NET_SUPPORTED] = "_NET_SUPPORTED",
    [NET_SUPPORTING_WM_CHECK] = "_NET_SUPPORTING_WM_CHECK",
    [NET_WM_NAME] = "_NET_WM_NAME",
    [NET_CLIENT_LIST] = "_NET_CLIENT_LIST",
    [NET_CLIENT_LIST_STACKING] = "_NET_CLIENT_LIST_STACKING",
    [NET_ACTIVE_WINDOW] = "_NET_ACTIVE_WINDOW",
    [NET_CLOSE_WINDOW] = "_NET_CLOSE_WINDOW",
    [NET_NUMBER_OF_DESKTOPS] = "_NET_NUMBER_OF_DESKTOPS",
    [NET_CURRENT_DESKTOP] = "_NET_CURRENT_DESKTOP",
    [NET_DESKTOP_GEOMETRY] = "_NET_DESKTOP_GEOMETRY",
    [NET_DESKTOP_VIEWPORT] = "_NET_DESKTOP_VIEWPORT",
    [NET_WORKAREA] = "_NET_WORKAREA",
    [NET_WM_DESKTOP] = "_NET_WM_DESKTOP",
    [NET_FRAME_EXTENTS] = "_NET_FRAME_EXTENTS",
    [NET_WM_STRUT] = "_NET_WM_STRUT",
    [NET_WM_STRUT_PARTIAL] = "_NET_WM_STRUT_PARTIAL",
    [NET_WM_STATE] = "_NET_WM_STATE",
    [NET_WM_STATE_HIDDEN] = "_NET_WM_STATE_HIDDEN",
    [NET_WM_STATE_ABOVE] = "_NET_WM_STATE_ABOVE",
    [NET_WM_STATE_BELOW] = "_NET_WM_STATE_BELOW",
    [NET_WM_STATE_SKIP_TASKBAR] = "_NET_WM_STATE_SKIP_TASKBAR",
    [NET_WM_STATE_SKIP_PAGER] = "_NET_WM_STATE_SKIP_PAGER",
    [NET_WM_STATE_STICKY] = "_NET_WM_STATE_STICKY",
    [NET_WM_STATE_DEMANDS_ATTENTION] = "_NET_WM_STATE_DEMANDS_ATTENTION",
    [NET_WM_WINDOW_TYPE] = "_NET_WM_WINDOW_TYPE",
    [NET_WM_WINDOW_TYPE_DESKTOP] = "_NET_WM_WINDOW_TYPE_DESKTOP",
    [NET_WM_WINDOW_TYPE_DOCK] = "_NET_WM_WINDOW_TYPE_DOCK",
    [NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    [NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
    [NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
    [NET_WM_WINDOW_TYPE_TOOLTIP] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
    [NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
    [NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
    [NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
    [NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL",
    [UTF8_STRING] = "UTF8_STRING",
    [WM_PROTOCOLS] = "WM_PROTOCOLS",
    [WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
    [WM_TAKE_FOCUS] = "WM_TAKE_FOCUS",
    [WM_STATE] = "WM_STATE",
    [WM_CHANGE_STATE] = "WM_CHANGE_STATE",
};

void ewmh_init_atoms(Display* display) {
    XInternAtoms(display, atomNames, NOIR_ATOM_COUNT, False, noirAtoms);
}

Window ewmh_setup_root(Display* display, Window root, int width, int height) {
    // Hints NoirWM implements. Everything advertised here is honoured.
    const NoirAtom supported[] = {
        NET_SUPPORTED, NET_SUPPORTING_WM_CHECK, NET_WM_NAME,
        NET_CLIENT_LIST, NET_CLIENT_LIST_STACKING, NET_ACTIVE_WINDOW, NET_CLOSE_WINDOW,
        NET_NUMBER_OF_DESKTOPS, NET_CURRENT_DESKTOP, NET_DESKTOP_GEOMETRY,
        NET_DESKTOP_VIEWPORT, NET_WORKAREA, NET_WM_DESKTOP, NET_FRAME_EXTENTS,
        NET_WM_STRUT, NET_WM_STRUT_PARTIAL,
        NET_WM_STATE, NET_WM_STATE_HIDDEN, NET_WM_STATE_ABOVE, NET_WM_STATE_BELOW,
        NET_WM_STATE_SKIP_TASKBAR, NET_WM_STATE_SKIP_PAGER, NET_WM_STATE_STICKY,
        NET_WM_STATE_DEMANDS_ATTENTION,
        NET_WM_WINDOW_TYPE, NET_WM_WINDOW_TYPE_DESKTOP, NET_WM_WINDOW_TYPE_DOCK,
        NET_WM_WINDOW_TYPE_NOTIFICATION, NET_WM_WINDOW_TYPE_POPUP_MENU,
        NET_WM_WINDOW_TYPE_DROPDOWN_MENU, NET_WM_WINDOW_TYPE_TOOLTIP,
        NET_WM_WINDOW_TYPE_SPLASH, NET_WM_WINDOW_TYPE_UTILITY,
        NET_WM_WINDOW_TYPE_DIALOG, NET_WM_WINDOW_TYPE_NORMAL,
    };
    const int supportedCount = (int)(sizeof(supported) / sizeof(supported[0]));
    Atom atoms[sizeof(supported) / sizeof(supported[0])];
    for (int i = 0; i < supportedCount; i++) {
        atoms[i] = noirAtoms[supported[i]];
    }
    ewmh_set_atoms(display, root, noirAtoms[NET_SUPPORTED], atoms, supportedCount);

    // _NET_SUPPORTING_WM_CHECK: a child window that names the WM, referenced
    // from both the root and itself, tells clients an EWMH WM is running.
    Window check = XCreateSimpleWindow(display, root, -1, -1, 1, 1, 0, 0, 0);
    ewmh_set_windows(display, root, noirAtoms[NET_SUPPORTING_WM_CHECK], &check, 1);
    ewmh_set_windows(display, check, noirAtoms[NET_SUPPORTING_WM_CHECK], &check, 1);
    const char* name = "NoirWM";
    XChangeProperty(display, check, noirAtoms[NET_WM_NAME], noirAtoms[UTF8_STRING], 8,
                    PropModeReplace, (const unsigned char*)name, (int)strlen(name));

    // A single desktop for now; the 3D cube workspaces come later.
    const long desktops = 1, current = 0;
    const long geometry[2] = { width, height };
    const long viewport[2] = { 0, 0 };
    const long workarea[4] = { 0, 0, width, height };
    ewmh_set_cardinals(display, root, noirAtoms[NET_NUMBER_OF_DESKTOPS], &desktops, 1);
    ewmh_set_cardinals(display, root, noirAtoms[NET_CURRENT_DESKTOP], &current, 1);
    ewmh_set_cardinals(display, root, noirAtoms[NET_DESKTOP_GEOMETRY], geometry, 2);
    ewmh_set_cardinals(display, root, noirAtoms[NET_DESKTOP_VIEWPORT], viewport, 2);
    ewmh_set_cardinals(display, root, noirAtoms[NET_WORKAREA], workarea, 4);

    Window none = None;
    ewmh_set_windows(display, root, noirAtoms[NET_ACTIVE_WINDOW], &none, 1);
    ewmh_set_windows(display, root, noirAtoms[NET_CLIENT_LIST], NULL, 0);
    ewmh_set_windows(display, root, noirAtoms[NET_CLIENT_LIST_STACKING], NULL, 0);
    return check;
}

int ewmh_get_atoms(Display* display, Window window, Atom property, Atom** out) {
    Atom type;
    int format;
    unsigned long count = 0, remaining;
    unsigned char* data = NULL;
    *out = NULL;
    if (XGetWindowProperty(display, window, property, 0, 1024, False, XA_ATOM, &type, &format,
                           &count, &remaining, &data) != Success || !data) {
        return 0;
    }
    if (type != XA_ATOM || format != 32) {
        XFree(data);
        return 0;
    }
    *out = (Atom*)data;
    return (int)count;
}

int ewmh_has_atom(Display* display, Window window, Atom property, Atom value) {
    Atom* atoms;
    int count = ewmh_get_atoms(display, window, property, &atoms);
    int found = 0;
    for (int i = 0; i < count && !found; i++) {
        found = atoms[i] == value;
    }
    if (atoms) XFree(atoms);
    return found;
}

void ewmh_set_atoms(Display* display, Window window, Atom property, const Atom* atoms, int count) {
    XChangeProperty(display, window, property, XA_ATOM, 32, PropModeReplace,
                    (const unsigned char*)atoms, count);
}

void ewmh_set_cardinals(Display* display, Window window, Atom property, const long* values, int count) {
    XChangeProperty(display, window, property, XA_CARDINAL, 32, PropModeReplace,
                    (const unsigned char*)values, count);
}

void ewmh_set_windows(Display* display, Window window, Atom property, const Window* windows, int count) {
    XChangeProperty(display, window, property, XA_WINDOW, 32, PropModeReplace,
                    (const unsigned char*)windows, count);
}

int ewmh_get_cardinals(Display* display, Window window, Atom property, long* out, int max) {
    Atom type;
    int format;
    unsigned long count = 0, remaining;
    unsigned char* data = NULL;
    if (XGetWindowProperty(display, window, property, 0, max, False, XA_CARDINAL, &type, &format,
                           &count, &remaining, &data) != Success || !data) {
        return 0;
    }
    int n = 0;
    if (type == XA_CARDINAL && format == 32) {
        // Xlib returns format-32 data as an array of long.
        const long* values = (const long*)data;
        for (; n < (int)count && n < max; n++) {
            out[n] = values[n];
        }
    }
    XFree(data);
    return n;
}

void ewmh_change_state(Display* display, Window window, long action, Atom state) {
    Atom* atoms;
    int count = ewmh_get_atoms(display, window, noirAtoms[NET_WM_STATE], &atoms);
    Atom* next = malloc(sizeof(Atom) * (size_t)(count + 1));
    int nextCount = 0, present = 0;
    for (int i = 0; i < count; i++) {
        if (atoms[i] == state) {
            present = 1;
            continue;
        }
        next[nextCount++] = atoms[i];
    }
    if (atoms) XFree(atoms);

    int keep = action == NET_WM_STATE_ADD || (action == NET_WM_STATE_TOGGLE && !present);
    if (keep) {
        next[nextCount++] = state;
    }
    ewmh_set_atoms(display, window, noirAtoms[NET_WM_STATE], next, nextCount);
    free(next);
}

int ewmh_supports_protocol(Display* display, Window window, Atom protocol) {
    Atom* protocols = NULL;
    int count = 0, found = 0;
    if (XGetWMProtocols(display, window, &protocols, &count)) {
        for (int i = 0; i < count && !found; i++) {
            found = protocols[i] == protocol;
        }
        XFree(protocols);
    }
    return found;
}
