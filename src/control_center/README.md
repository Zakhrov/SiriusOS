# Noir Control Center (NoirControlCenter)

The system settings application of the Noir Desktop Environment, in the
spirit of KDE's System Settings: one window with categories on the left and
the selected page on the right. It uses the same design language as the
shell (black glass, silver edges, no rounded corners, colours that invert on
hover and keyboard focus, 3D presses and page turns) and renders with Vulkan
through the shared `Noir.Ui` module.

## Pages

| Page | What it does | Talks to |
| --- | --- | --- |
| Appearance | Reduce motion, animation duration, glass opacity and blur (with a live preview), desktop icons, widgets and grid | `NoirConfig` (the shell follows live) |
| Notifications | Do not disturb, how long pop-ups stay, test notification | `NoirConfig`, `org.freedesktop.Notifications` |
| Default Applications | Web browser, file manager, e-mail, text editor, image viewer, video and music player, terminal | `~/.config/noir-mimeapps.list` (XDG, Noir only), `NoirConfig` for the terminal |
| Display | Monitors: on/off, primary, resolution, refresh rate, orientation, with a 15-second keep-or-revert; backlight brightness | RandR (xcb), logind `SetBrightness` |
| Sound | Output and input devices, their volume, mute, default and connector; per-application volume | PulseAudio protocol via `pactl` (PulseAudio or PipeWire) |
| Input Devices | Key repeat delay and rate, up to four keyboard layouts and a switch shortcut; mouse and touchpad speed, acceleration, natural scrolling, left-handed, tap to click, middle click, disable while typing | XKB (xcb), `setxkbmap`, libinput properties over XInput2 (xcb) |
| Power | Batteries (charge, time left, draw, health), power profile, blank and turn off the screen after, suspend / hibernate / restart / shut down | UPower, power-profiles-daemon, X screen saver and DPMS (xcb), logind |
| Graphics | Vulkan devices and drivers, the one the stack renders with and why; preferred GPU, NoirWM's compositor, Vulkan validation | Vulkan, the shared device picker, `~/.config/noir/session.conf` |
| Network | Networking and Wi-Fi switches, devices with addresses, Wi-Fi scan / connect (WPA personal or open) / disconnect / forget | NetworkManager; read-only interface list without it |
| Bluetooth | Power, search, pair, connect, disconnect, remove | BlueZ |
| Date & Time | Time zone, network time, setting the clock; 24-hour clock, seconds, date in the taskbar | systemd-timedated, `NoirConfig` |
| About This System | OS, Noir and Qt versions, kernel, window system, machine, processor, memory, graphics, displays, storage; copy as text | `/etc/os-release`, `/proc`, DMI, Vulkan, `QStorageInfo` |

### Apply, Reset, Defaults

As in KDE's System Settings, pages that change settings stage the change:
nothing happens until **Apply** (or Ctrl+S) in the footer. **Reset** drops the
changes not applied yet and **Defaults** stages the Noir defaults for the
page. Leaving the page, or closing the window, with changes not applied asks
whether to apply or discard them. Display's Apply is followed by its 15-second
keep-or-revert countdown in the footer, and the page cannot be left until it
is kept or reverted.

Staged: Appearance, Notifications, Default Applications, Display, Input
Devices, Power, Graphics, Date & Time. Immediate, as in KDE: Sound (volumes),
Network, Bluetooth, the Power page's suspend / restart / shut down buttons,
and setting the clock by hand.

Every page works without its service and says what is missing instead (no
NetworkManager, no BlueZ, no sound server, no DPMS, ...). Any X extension is
checked for before it is used: a request to a missing one would close the
whole X connection.

## Running

```bash
NoirControlCenter                    # opens on Appearance
NoirControlCenter --page display     # about, appearance, display, sound, network, bluetooth,
                                     # power, input, datetime, notifications, apps, graphics
NoirControlCenter --apply            # re-apply saved hardware settings and exit
```

There is only ever one window: the first instance owns
`org.siriusos.NoirControlCenter` on the session bus, and starting it again
asks that window to show the requested page (`ShowPage(s)`). The shell uses
this: the desktop's Settings icon and context menu, the start menu, the tray
icons (network, sound, battery) and the clock all open the matching page.

Keyboard: Ctrl+F or `/` searches, Up/Down in the sidebar changes page,
Enter or Tab goes into the page, and the page scrolls to keep the focused
control in view. Ctrl+W closes the window.

## Where settings go

| Settings | Stored in | Applied |
| --- | --- | --- |
| Appearance, clock, notifications, terminal | `~/.config/SiriusOS/Noir.ini` | On Apply; every Noir program watches the file and follows at once |
| Displays, keyboard repeat and layouts, pointer options, screen blanking | `~/.config/SiriusOS/Noir.ini` | On Apply, and again at login: `noir-session` runs `NoirControlCenter --apply` (no window, ~25 ms) after NoirWM starts |
| Preferred GPU, compositor, validation | `~/.config/noir/session.conf` | At the next login (only the variables it manages are rewritten) |
| Default applications | `~/.config/noir-mimeapps.list`, so other desktops keep their own defaults | At once (XDG) |
| Time zone, NTP, power profile | The system services themselves | On Apply |
| Network, Bluetooth | The system services themselves | At once |

A new display mode is saved only after **Keep**; without it the previous mode
comes back after 15 seconds. NoirWM and its compositor follow the new screen
size, and the shell moves its desktop and taskbar with it.

## Layout

```
src/control_center/
├── src/
│   ├── main.cpp               entry point, --page / --apply, Vulkan setup
│   ├── ControlCenterService.* single instance on the session bus
│   ├── Platform.*             X connection and extension checks, D-Bus calls, session.conf, processes
│   ├── SystemInfo.*           About
│   ├── GraphicsConfig.*       Graphics (and the GPU list for About)
│   ├── DisplayConfig.*        Display: RandR and backlight
│   ├── AudioConfig.*          Sound
│   ├── NetworkConfig.*        Network
│   ├── BluetoothConfig.*      Bluetooth
│   ├── PowerConfig.*          Power
│   ├── InputConfig.*          Input Devices
│   ├── DateTimeConfig.*       Date & Time
│   └── DefaultApps.*          Default Applications
└── qml/
    ├── Main.qml               window, page registry, header, page transitions
    ├── Sidebar.qml            categories and search
    ├── SettingsPage.qml, Section.qml, SettingRow.qml, InfoRow.qml, Banner.qml
    └── pages/                 one file per page
```

D-Bus calls go out as plain messages with timeouts (never `QDBusInterface`,
whose constructor introspects synchronously). Calls that change the system
are asynchronous and allow interactive polkit authorisation.

## Limitations

- **Administrator actions need a polkit agent.** Changing the time zone, NTP
  or the clock, and some network or power actions, ask polkit; the Noir
  session does not run an authentication agent yet, so where polkit wants a
  password the page reports that instead.
- Displays are placed side by side, left to right; there is no drag-and-drop
  arrangement, mirroring or scaling yet.
- Wi-Fi supports open and WPA/WPA2/WPA3 personal networks; enterprise
  (802.1X) networks and VPNs are not configured here.
- Pointer options apply to every device of a class (all mice, all touchpads).

## Testing

`tools/xvfb/noir-xvfb smoke` (and `ctest`) opens the Control Center in the
headless session, checks that a second start switches the open window's page
and that a changed setting reaches the running shell. The harness keeps its
own `XDG_CONFIG_HOME`, so trying pages there never touches your real
settings — but the system services (NetworkManager, BlueZ, timedated, UPower)
are the host's, so switching things on those pages changes the host.

```bash
tools/xvfb/noir-xvfb start --size 1600x900
tools/xvfb/noir-xvfb spawn NoirControlCenter --page about
tools/xvfb/noir-xvfb shot
```
