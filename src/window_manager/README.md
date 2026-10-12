# SiriusOS Noir Window Manager (NoirWM)

This repository contains the source code for the SiriusOS Noir Desktop Environment's custom window manager, NoirWM. NoirWM is designed to provide a sleek, cyberpunk-inspired user experience with a focus on sharp edges, 3D effects, and a glass-like aesthetic.

## Building and Running

### Dependencies

- Vulkan SDK (headers, loader and `glslangValidator`)
- X11 development libraries, including Xft, Xcomposite, Xdamage, Xfixes and Xext
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

   NoirWM composites with Vulkan by default. If Vulkan cannot present to the
   X server it says so and carries on as a plain window manager; pass
   `--no-compositor` to skip the attempt:

   ```bash
   NoirWM --no-compositor &
   NoirShell
   ```

## Window decorations

Every normal window is reparented into a frame drawn in the Noir palette
(black and silver, sharp corners):

- **Title bar**: window icon (`_NET_WM_ICON`) and title on the left;
  minimize, maximize/restore and close on the right. The controls invert
  under the pointer and sink when pressed, as the VDD asks.
- **Move**: drag the title bar. **Resize**: drag any border or corner
  (`WM_NORMAL_HINTS` minimum, maximum and increments are honoured, so
  terminals resize by whole cells).
- **Maximize / restore**: the middle button, a double-click on the title bar,
  or `_NET_WM_STATE_MAXIMIZED_*` requests. A maximized window fills the work
  area and follows it when the taskbar's strut changes.
- New windows that do not ask for a position cascade from the top-left of
  the work area.

Windows typed as desktop, dock, menu, notification, tooltip or splash, and
windows whose `_MOTIF_WM_HINTS` turn decorations off, get no frame.

Keyboard: `Alt+Tab` switches windows, `Alt+F4` closes, `Alt+F9` minimizes and
`Alt+F10` maximizes or restores the active window.

The code is in [`src/Frame.c`](src/Frame.c); title text is drawn with Xft.

## Compositor

The compositor ([`../compositor`](../compositor)) runs inside NoirWM on the
same X connection:

- All top-level windows are redirected off screen (`Composite`, manual mode).
- `Damage` reports what changed; the damaged rectangle of the window's
  pixmap is read back over MIT-SHM into a Vulkan texture.
- The stack is drawn bottom to top onto the composite overlay window:
  depth-32 windows are blended as premultiplied alpha, framed windows get a
  soft drop shadow, and frames are paced to 60 Hz and only drawn when
  something changed.
- NoirWM owns `_NET_WM_CM_S0`, which is how clients (NoirShell included)
  learn that translucent windows will be blended.

Any Vulkan driver that can present to the X server works, including Mesa's
software driver (lavapipe). `NOIR_VK_DEVICE=<index>` picks a device and
`NOIR_VK_VALIDATION=1` enables the validation layer.

Window contents travel through the CPU, which is simple and driver
independent but not free; a zero-copy path (DRI3 pixmaps imported as
dma-bufs) is the natural next step for hardware drivers.

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
| Maximize | `_NET_WM_STATE_MAXIMIZED_VERT` / `_HORZ` (always both together), also honoured when set before mapping |
| Frames | Title bar and borders reported in `_NET_FRAME_EXTENTS` (4, 4, 28, 4); `_NET_REQUEST_FRAME_EXTENTS`; `_NET_WM_ALLOWED_ACTIONS`; `_NET_WM_STATE_FOCUSED` |
| Compositing | `_NET_WM_CM_S0` selection owned while the compositor runs |

Docks only take focus when a pager/taskbar asks (source indication 2), so
clicking the taskbar never makes it the active window.

Not implemented yet: multiple desktops (the 3D cube), fullscreen,
`_NET_WM_MOVERESIZE` / `_NET_MOVERESIZE_WINDOW`, `_NET_WM_PING`,
`_NET_WM_USER_TIME` focus-stealing prevention, and the VDD's window
animations (wobble, magic lamp, shatter, flip switcher).
