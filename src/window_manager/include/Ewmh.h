#pragma once

#include <X11/Xlib.h>

// Atoms NoirWM uses for EWMH (Extended Window Manager Hints) and ICCCM.
typedef enum {
    NET_SUPPORTED,
    NET_SUPPORTING_WM_CHECK,
    NET_WM_NAME,
    NET_CLIENT_LIST,
    NET_CLIENT_LIST_STACKING,
    NET_ACTIVE_WINDOW,
    NET_CLOSE_WINDOW,
    NET_NUMBER_OF_DESKTOPS,
    NET_CURRENT_DESKTOP,
    NET_DESKTOP_GEOMETRY,
    NET_DESKTOP_VIEWPORT,
    NET_WORKAREA,
    NET_WM_DESKTOP,
    NET_FRAME_EXTENTS,
    NET_WM_STRUT,
    NET_WM_STRUT_PARTIAL,
    NET_WM_STATE,
    NET_WM_STATE_HIDDEN,
    NET_WM_STATE_ABOVE,
    NET_WM_STATE_BELOW,
    NET_WM_STATE_SKIP_TASKBAR,
    NET_WM_STATE_SKIP_PAGER,
    NET_WM_STATE_STICKY,
    NET_WM_STATE_DEMANDS_ATTENTION,
    NET_WM_WINDOW_TYPE,
    NET_WM_WINDOW_TYPE_DESKTOP,
    NET_WM_WINDOW_TYPE_DOCK,
    NET_WM_WINDOW_TYPE_NOTIFICATION,
    NET_WM_WINDOW_TYPE_POPUP_MENU,
    NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
    NET_WM_WINDOW_TYPE_TOOLTIP,
    NET_WM_WINDOW_TYPE_SPLASH,
    NET_WM_WINDOW_TYPE_UTILITY,
    NET_WM_WINDOW_TYPE_DIALOG,
    NET_WM_WINDOW_TYPE_NORMAL,
    UTF8_STRING,
    WM_PROTOCOLS,
    WM_DELETE_WINDOW,
    WM_TAKE_FOCUS,
    WM_STATE,
    WM_CHANGE_STATE,
    NOIR_ATOM_COUNT
} NoirAtom;

// _NET_WM_STATE client message actions.
#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD    1
#define NET_WM_STATE_TOGGLE 2

extern Atom noirAtoms[NOIR_ATOM_COUNT];

void ewmh_init_atoms(Display* display);
// Publishes _NET_SUPPORTED, the WM check window and the desktop properties.
Window ewmh_setup_root(Display* display, Window root, int width, int height);

// Returns the number of atoms read; *out must be freed with XFree (may be NULL).
int ewmh_get_atoms(Display* display, Window window, Atom property, Atom** out);
int ewmh_has_atom(Display* display, Window window, Atom property, Atom value);
void ewmh_set_atoms(Display* display, Window window, Atom property, const Atom* atoms, int count);
void ewmh_set_cardinals(Display* display, Window window, Atom property, const long* values, int count);
void ewmh_set_windows(Display* display, Window window, Atom property, const Window* windows, int count);
// Reads up to `max` CARDINALs into `out`; returns the number read.
int ewmh_get_cardinals(Display* display, Window window, Atom property, long* out, int max);
// Adds, removes or toggles one atom in a window's _NET_WM_STATE.
void ewmh_change_state(Display* display, Window window, long action, Atom state);
int ewmh_supports_protocol(Display* display, Window window, Atom protocol);
