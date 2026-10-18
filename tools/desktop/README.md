# Try the desktop yourself (`noir-desktop`)

A harness for people: the whole Noir session — NoirWM with its Vulkan
compositor, NoirShell, the Control Center — running from the build tree with
your real GPU, keyboard and mouse, in a window on your current desktop.

```bash
tools/desktop/noir-desktop start          # builds first; add --fullscreen to cover the screen
tools/desktop/noir-desktop status
tools/desktop/noir-desktop stop           # or just close the window
```

## Why it is representative of bare metal

The other harnesses trade realism for convenience: Xvfb has no GPU path
(frames are copied through the CPU, translucency is off) and the QEMU guest
renders on the CPU unless the host has Venus. `noir-desktop` runs Noir on a
**rootful Xwayland with glamor**: a complete X screen in one window, driven
by your GPU and offering **DRI3**. That gives Noir the same path it takes on
an installed system:

| | `noir-desktop` | Bare metal (Xorg) |
| --- | --- | --- |
| Vulkan device | Your GPU, chosen by the same rules (the one driving the X server, `--gpu` to override) | Same |
| Presentation | Direct from the GPU (DRI3) | Same |
| NoirWM compositor | On, translucent glass and shadows | Same |
| Input | Your keyboard and mouse, through the window | Same devices, directly |
| Session | `noir-session` from the build tree, with its own session bus | `noir-session` from the display manager |

`status` checks this rather than assuming it:

```
display      :1, 1632x918 (Xwayland in kwin_wayland, nested in an X11 window)
DRI3         yes: Vulkan presents straight from the GPU, as on bare metal
window mgr   compositing with Vulkan on AMD Radeon Graphics (RADV RENOIR)
shell        rendering with Vulkan 1.0 on AMD Radeon Graphics (RADV RENOIR) (drives the X server)
glass        translucent, blended by NoirWM's compositor
```

Where it is not bare metal:

- **The X server is Xwayland, not Xorg's modesetting driver.** Display
  modes cannot really be changed (the screen is the window), so try the
  Display page's modes in the QEMU guest (`tools/vm`) or on bare metal.
  Input goes through the host's libinput, so the Control Center's mouse and
  touchpad options have no XInput2 devices to adjust here.
- **Your desktop sees the keyboard first.** Global shortcuts of the host
  desktop (in KDE, for instance, Meta or Alt+F1) may be taken before they
  reach Noir; the start button always works, and `--fullscreen` makes it
  less likely that the host takes keys.
- **System services are the host's.** Network, Bluetooth, power, date and
  time and sound in the Control Center act on this machine for real.

For the real thing, log in to it — see [Bare metal](#bare-metal) below.

## How it is put together

```
your desktop (X11)
└── kwin_wayland --x11-display      a nested Wayland compositor in a window (or weston)
    └── Xwayland -fullscreen        rootful X screen, glamor + DRI3 on your GPU   → DISPLAY=:N
        ├── dbus-daemon --session   the session's own bus (the shell is its notification daemon)
        └── noir-dev-session        → noir-session → NoirWM, NoirControlCenter --apply, NoirShell
```

On a Wayland desktop the nested compositor is not needed: Xwayland opens
its window on your desktop directly.

[`noir-dev-session`](noir-dev-session) runs `noir-session` with the build
tree's binaries on `PATH` and the Control Center listed in the start menu.
The login-screen entry below runs the very same script.

## Commands

| Command | |
| --- | --- |
| `start [options]` | Build, then start everything and print `status` |
| `restart [--no-build]` | Rebuild and restart the Noir session in the same window — the edit-and-look loop |
| `stop` | Close it all (closing the window does too) |
| `status` | Display, DRI3, the Vulkan device of NoirWM and NoirShell, whether the glass is translucent |
| `logs [shell\|wm\|control-center\|session\|xwayland\|compositor]` | The last lines of a log |
| `shot [FILE]` | Screenshot of the Noir screen |
| `run CMD...` | Start a program inside the session: `run vkcube`, `run xterm`, `run NoirControlCenter --page display` |
| `env` | Exports for a terminal of your own: `eval "$(noir-desktop env)"; wmctrl -l` |
| `install-session` / `remove-session` | The login-screen entry (sudo) |

`start` options:

| Option | |
| --- | --- |
| `--size WxH` | Window size; default 85% of your screen, at most 1920×1080 |
| `--fullscreen` | Cover the whole screen (leave with `noir-desktop stop` from another terminal, or your desktop's window shortcuts) |
| `--no-build` | Use the build tree as it is |
| `--clean-profile` | Settings in `build/desktop/profile` instead of your `~/.config` (kept between runs; delete the directory to reset) |
| `--gpu NAME` | Render on the GPU whose Vulkan name contains NAME (`--gpu navi`, `--gpu renoir`); sets `NOIR_VK_DEVICE` |
| `--validation` | Vulkan validation layer on, as `NOIR_VK_VALIDATION=1` |
| `--no-compositor` | NoirWM without its compositor (opaque surfaces) |
| `--demo` | Sample tasks and notifications in the shell |

### Your settings

By default the session uses your own `~/.config`, as Noir would after you
log in to it: what you set in the Control Center stays for the next run and
for a bare-metal session. Noir keeps its settings to its own files
(`SiriusOS/Noir.ini`, `noir/session.conf`, and `noir-mimeapps.list` for
default applications), so your current desktop's are left alone.
`--clean-profile` starts from a blank, separate profile instead.

Logs are kept in `build/desktop/state/noir/` (shell, wm, control-center),
the harness's own in `build/desktop/`.

## Bare metal

To run the build on the machine itself, outside any window:

```bash
tools/desktop/noir-desktop install-session   # adds /usr/share/xsessions/noir-dev.desktop (sudo)
```

Log out, pick **Noir (development build)** at the login screen, and log in.
The display manager starts Xorg on your GPU and runs `noir-dev-session`, so
this is exactly what an installed Noir does, with the build tree as the
installation: rebuild, log out and back in to try new code. Everything there
is real — display modes, input devices, DPMS. `remove-session` takes the
entry away again. (`cmake --install` installs Noir properly instead, with its
own `Noir` session entry.)

## Requirements

`Xwayland` (with glamor, which any distribution's build has), a nested Wayland
compositor when your desktop is X11 — `kwin_wayland` (KDE) or `weston` —,
`dbus-daemon`, `xdpyinfo`, `wmctrl` for `--fullscreen`, and ImageMagick's
`import` for `shot`. Plus a GPU with a Vulkan driver; with none, Noir falls
back to lavapipe and `status` says the path is not representative.
