# SiriusOS Noir Shell (NoirShell)

NoirShell is the desktop shell of the Noir Desktop Environment: the desktop
surface, taskbar, start menu, system tray and notifications. It follows
[`VDD.md`](../../VDD.md) and is built in C++ with Qt 6 and QML, rendering
through Qt Quick's **Vulkan** RHI on **X11**.

## What's in it

| Surface | Details |
| --- | --- |
| Desktop | Black-to-graphite wallpaper with a receding silver perspective grid; bevelled 3D launcher icons (Home, Terminal, Settings, Trash); glass context menu on right-click / Menu key / Shift+F10, with shortcuts into the Control Center |
| Desktop widgets | Chamfered-polygon glass widgets (Chronometer, System monitor with CPU trace and memory gauge) that float with a slow bob and a counter-moving soft shadow; draggable |
| Taskbar | Black-glass bar at the bottom: start button, live task list (EWMH), system tray (network, volume, battery, notification counter) and clock in the bottom-right corner. The tray icons and the clock open the matching Control Center page |
| Start menu | Glass launcher that folds up out of the taskbar in 3D; lists XDG `.desktop` applications, type-to-search, arrow keys + Enter, Control Center and power menu |
| Notifications | `org.freedesktop.Notifications` D-Bus server; cards slide and swing in from the bottom right and slide out when dismissed; actions, urgency and progress (`value` hint) supported |
| Tooltips | Own override-redirect windows so they can rise above the taskbar; pop up with a 3D tilt and a bounce |

Design rules carried over from the VDD:

- Black with silver/white highlights, glass transparency, **no rounded corners**
  (`Theme.radius` is 0 everywhere).
- Interactive elements **invert** (white background, black foreground) on hover
  *and* on keyboard focus.
- Buttons and menu items have a 3D press (the face sinks and its bevel flips).
- First-party icons are monochrome SVGs in [`src/ui/icons/`](../ui/icons);
  facet depth is encoded in opacity so they keep their 3D shading when tinted
  silver or black. Third-party application icons are shown untouched.
- Everything is reachable from the keyboard. Reduce motion (desktop context
  menu, or Control Center > Appearance) collapses all animations.

## Settings

The shell reads its settings from `NoirConfig` (`~/.config/SiriusOS/Noir.ini`,
see [`src/ui`](../ui/src/NoirConfig.h)) and follows changes live, so whatever
the [Control Center](../control_center/README.md) changes shows up at once:
reduced motion and animation speed, glass opacity, desktop icons, widgets and
grid, the clock format, do not disturb (cards stay off the screen, the bell
still counts them), how long notifications stay, and the terminal used by the
Terminal launcher and by terminal applications.

## Layout

```
src/shell/
├── src/                     C++ backends exposed to QML (module "Noir.Shell")
│   ├── main.cpp             entry point, Vulkan RHI + xcb selection, CLI flags
│   ├── X11Integration.*     EWMH window types, _NET_WM_STATE, dock struts
│   ├── TaskModel.*          _NET_CLIENT_LIST task list + _NET_WM_ICON provider
│   ├── AppModel.*           XDG .desktop parser, search filter, theme icons
│   ├── SystemMonitor.*      /proc and /sys poller (CPU, memory, battery, net)
│   ├── DebugInterface.*     org.siriusos.NoirShell.Debug (--debug-interface)
│   └── NotificationServer.* org.freedesktop.Notifications server + model
└── qml/                     the shell's surfaces and widgets
```

The theme, the shared controls (buttons, tooltips, menus, switches, sliders,
drop-downs, ...), the icons, `NoirConfig` and the Vulkan setup live in the
`Noir.Ui` module in [`src/ui`](../ui), which the Control Center uses too.

`Main.qml` picks a host:

- **SessionShell.qml** – the real session. Each role is its own X11 window,
  typed through EWMH so the window manager can stack and composite it:
  desktop (`_NET_WM_WINDOW_TYPE_DESKTOP`, below), taskbar
  (`_NET_WM_WINDOW_TYPE_DOCK` + `_NET_WM_STRUT_PARTIAL`), start menu
  (`POPUP_MENU`) and notifications (`NOTIFICATION`).
- **WindowedShell.qml** – development mode: the whole shell composed inside
  one ordinary window.

## Building

Dependencies (Ubuntu/Debian names): `qt6-base-dev qt6-declarative-dev
qt6-svg-dev libxcb1-dev libvulkan-dev`, plus the runtime QML modules
`qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-shapes
qml6-module-qt5compat-graphicaleffects qml6-module-qtqml-workerscript`.
Qt 6.4 or newer is required.

```bash
cmake -B build -S .
cmake --build build --target NoirShell
```

## Running

```bash
# Development: everything in one 1280x720 window, with sample tasks and notifications
./build/src/shell/NoirShell --windowed 1280x720 --demo

# Session: separate desktop / dock / popup surfaces (start after the window manager)
./build/src/shell/NoirShell
```

Keyboard: `Super` (windowed) or `Alt+F1` / `Meta+A` toggles the start menu,
`Tab` moves between areas, arrow keys move within them, `Enter`/`Space`
activate, `Esc` closes menus, `Delete` dismisses a focused notification.
In the taskbar, clicking the active task minimizes it and middle-click closes
it.

The shell renders with Vulkan only; there is no OpenGL or raster fallback,
and `QSG_RHI_BACKEND` / `QT_QUICK_BACKEND` are ignored. Like the rest of the
stack it targets Vulkan 1.0, so it runs on the widest range of hardware. It
creates its own Vulkan 1.0 `QVulkanInstance` and hands it, with the chosen
device, to every window as its platform window is created. Without a Vulkan
device that can present to the X server, it exits.

The device is chosen by the same code NoirWM's compositor uses
([`src/vulkan_device`](../vulkan_device/include/NoirVulkanDevice.h)), so the
two always share a GPU. Among the devices that can render and present to
the X server, it takes, in order:

1. the one `NOIR_VK_DEVICE` names: a substring of the device name
   (`NOIR_VK_DEVICE=renoir`) or an index in enumeration order;
2. the one driving the X server: the device node DRI3 hands out, matched
   with `VK_EXT_physical_device_drm` (skipped when the loader lacks
   `VK_KHR_get_physical_device_properties2`, which a 1.0 instance needs to
   query it);
3. the best device type (discrete, integrated, virtual, other, CPU), then the
   lowest vendor and device ID.

| Device | Presentation |
| --- | --- |
| GPU, X server with DRI3 (virtio-gpu Venus in a VM included) | Direct; translucent surfaces when a compositor runs |
| GPU, X server without DRI3 (Xvfb with `MESA_VK_WSI_DEBUG=sw`) | Frames copied to the server; surfaces stay opaque |
| CPU (lavapipe) | Frames copied to the server; surfaces stay opaque, because Mesa's software presentation cannot target ARGB windows |

The choice and the rule that decided it are logged at startup, and
`QSG_INFO=1` prints the full renderer details. `NOIR_VK_VALIDATION=1`
enables the validation layer.

The taskbar, start menu and notification surfaces ask for a translucent
(ARGB) window only when a compositing manager owns `_NET_WM_CM_S0` and the
renderer can present to one; otherwise they are opaque black around their
content. The desktop surface is always opaque.

### Glass

Glass surfaces take their density from Control Center > Appearance (Glass
opacity) and are **frosted** unless Blur behind glass is off:

| Surface | What shows through | Blurred by |
| --- | --- | --- |
| Taskbar, start menu, notification cards | The windows behind them | NoirWM's compositor: the shell sets KDE's `_KDE_NET_WM_BLUR_BEHIND_REGION` on those windows (`X11Integration.setBlurBehind`), for the card rectangles only, so the gaps between cards stay clear |
| Desktop widgets | The wallpaper (windows never go behind them) | The shell itself (MultiEffect on the wallpaper, cut to the widget's chamfered outline) |

Every pane also carries a diagonal reflection (`GlassReflection`), which is
what makes black glass read as glass even over black. The wallpaper has a
faint silver haze in the sky for the widgets' glass to catch.

## Headless inspection and debugging

[`tools/xvfb/noir-xvfb`](../../tools/xvfb/README.md) boots NoirWM and the
shell on a private Xvfb display and D-Bus session, and lets you screenshot
it, dump the Qt Quick item tree, evaluate QML and inject input:

```bash
tools/xvfb/noir-xvfb start
tools/xvfb/noir-xvfb click '@toolTip="Applications"'
tools/xvfb/noir-xvfb shot
tools/xvfb/noir-xvfb stop
```

It relies on `--debug-interface`, which exports
`org.siriusos.NoirShell.Debug` at `/Debug` on the session bus with
`Windows()`, `ItemTree(window, visibleOnly)`, `Grab(window, path)` and
`Eval(expression)`. `Eval` runs arbitrary QML/JavaScript inside the shell, so
the interface is off unless the flag is given.

Try a notification from another process:

```bash
notify-send -u critical "Hello" "From org.freedesktop.Notifications"
```

## Running as a session

`cmake --install` adds [`noir-session`](../session/noir-session) and an
`xsessions/noir.desktop` entry, so a display manager can start the whole
desktop (NoirWM, then the shell). [`tools/vm/noir-vm`](../../tools/vm/README.md)
builds an openSUSE Leap 15.6 QEMU guest that boots into it through LightDM.

## Integration with NoirWM and the compositor

The shell talks to the window manager purely through EWMH, which NoirWM
implements (see [the NoirWM README](../window_manager/README.md#ewmh-support)):
the taskbar reads `_NET_CLIENT_LIST` / `_NET_ACTIVE_WINDOW` and sends
`_NET_ACTIVE_WINDOW`, `_NET_CLOSE_WINDOW` and `WM_CHANGE_STATE`; NoirWM keeps
the desktop at the bottom, the taskbar and popups on top, and reserves the
taskbar's strut in `_NET_WORKAREA`.

NoirWM composites by default, so the glass surfaces are blended over the
windows behind them:

```bash
# in ~/.xinitrc, or use noir-session
NoirWM &
exec NoirShell
```

Start the shell after NoirWM: it checks for the compositing manager once, at
startup. Under any other EWMH window manager, or with `NoirWM
--no-compositor`, the shell still works with opaque surfaces.

The window-level effects in the VDD (wobbly move, magic-lamp minimize,
shattering close, 3D cube workspaces, flip carousel task switching, dialog
expansion) belong to the compositor rather than the shell, and are not
implemented yet.

## Next steps

- Global `Super` key grab in session mode
- XEmbed / StatusNotifierItem tray icons for third-party applications
- Workspace switcher (backed by `_NET_NUMBER_OF_DESKTOPS` / `_NET_CURRENT_DESKTOP`)
- Quick volume popup, calendar popup and a notification history panel
- Audio cues listed in the VDD
- Multi-monitor: one desktop and taskbar per screen
