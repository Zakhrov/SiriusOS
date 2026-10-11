# SiriusOS Noir Window Manager (NoirWM)

This repository contains the source code for the SiriusOS Noir Desktop Environment's custom window manager, NoirWM. NoirWM is designed to provide a sleek, cyberpunk-inspired user experience with a focus on sharp edges, 3D effects, and a glass-like aesthetic.

## Building and Running

### Dependencies

- Vulkan SDK
- X11 development libraries
- CMake
- A C compiler (e.g., GCC or Clang)
- Make/Ninja

### Build Instructions

1. Clone the repository:

   ```bash
   git clone https://github.com/Zakhrov/SiriusOS.git
   ```

2. Navigate to the project directory and create a build directory:

   ```bash
   cd SiriusOS
   cmake -B build -S .
   ```

3. Build the project:

   ```bash
   cmake --build build
   ```

4. Run the window manager (ensure you are in an X11 session):

   ```bash
   xinit ./build/src/window_manager/NoirWM
   ```

   The Vulkan compositor does not draw client windows yet. To use NoirWM as a
   plain (non-compositing) window manager, for example with the Noir Shell
   taskbar, pass `--no-compositor`:

   ```bash
   NoirWM --no-compositor &
   NoirShell
   ```

## EWMH support

NoirWM implements the parts of the
[EWMH](https://specifications.freedesktop.org/wm-spec/latest/) and ICCCM specs
that taskbars and pagers rely on. Everything listed here is advertised in
`_NET_SUPPORTED`.

| Area | What NoirWM does |
| --- | --- |
| WM detection | `_NET_SUPPORTING_WM_CHECK` window named `NoirWM`; refuses to start if another WM owns the root |
| Client lists | `_NET_CLIENT_LIST` (mapping order) and `_NET_CLIENT_LIST_STACKING` (bottom to top) |
| Focus | `_NET_ACTIVE_WINDOW` kept current; click-to-focus; honours `WM_HINTS` input and `WM_TAKE_FOCUS`; focus falls back to the topmost window when the active one closes or minimizes |
| Requests | `_NET_ACTIVE_WINDOW` (raises, restores, focuses), `_NET_CLOSE_WINDOW` (`WM_DELETE_WINDOW`, else kill), `WM_CHANGE_STATE` → iconify, `_NET_WM_STATE` add/remove/toggle |
| Window state | `_NET_WM_STATE_HIDDEN` for minimized windows, `ABOVE`, `BELOW`, `SKIP_TASKBAR`, `SKIP_PAGER`, `STICKY`, `DEMANDS_ATTENTION` (also set from the ICCCM urgency hint); `WM_STATE` Normal/Iconic/Withdrawn |
| Window types | `DESKTOP` stays at the bottom; `DOCK`, `NOTIFICATION`, `POPUP_MENU`, `DROPDOWN_MENU`, `TOOLTIP` stay on top; these and `SPLASH` get no border |
| Struts | `_NET_WM_STRUT` / `_NET_WM_STRUT_PARTIAL` feed `_NET_WORKAREA`; new windows are kept inside the work area |
| Desktops | One desktop: `_NET_NUMBER_OF_DESKTOPS`, `_NET_CURRENT_DESKTOP`, `_NET_DESKTOP_GEOMETRY`, `_NET_DESKTOP_VIEWPORT`, `_NET_WM_DESKTOP` |
| Frames | 1 px silver border on normal windows, reported in `_NET_FRAME_EXTENTS` |

Docks only take focus when a pager/taskbar asks (source indication 2), so
clicking the taskbar never makes it the active window.

Not implemented yet: multiple desktops (the 3D cube), fullscreen and
maximized states, `_NET_MOVERESIZE_WINDOW`, `_NET_WM_PING`, and
`_NET_WM_USER_TIME` focus-stealing prevention.
