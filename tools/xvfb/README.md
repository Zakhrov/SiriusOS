# Xvfb harness (`noir-xvfb`)

Runs the Noir session (NoirWM + NoirShell) on a headless
[Xvfb](https://www.x.org/releases/current/doc/man/man1/Xvfb.1.xhtml) display
so it can be inspected and driven from a terminal, a script or CI, without
touching the desktop you are logged into.

Each session gets:

- its own X display (first free number, or `--display :N`);
- a private D-Bus session bus with service activation disabled, so the shell
  owns `org.freedesktop.Notifications` and nothing from the host session
  (portals, gvfs, the real notification daemon) is involved;
- `NoirWM --no-compositor` as the window manager (decorations, no blending;
  `--compositor` turns blending on, see below);
- `NoirShell --debug-interface`, which exports `org.siriusos.NoirShell.Debug`
  for item-tree dumps, surface grabs and QML evaluation.

## Requirements

`Xvfb`, `dbus-daemon`, `dbus-send`, `xprop`, plus `import` (ImageMagick) for
screenshots. Optional: `wmctrl` (nicer `status`), `notify-send` or `gdbus`
(`notify`), `x0vncserver` from TigerVNC (`vnc`), and the libXtst development
headers to build the `noir-xinject` input injector.

## Quick start

```bash
cmake -B build -S . && cmake --build build

tools/xvfb/noir-xvfb start            # 1280x720 session
tools/xvfb/noir-xvfb spawn xterm      # a real client for the taskbar
tools/xvfb/noir-xvfb status           # pids, EWMH root properties, windows
tools/xvfb/noir-xvfb shot             # -> build/xvfb/shots/screen-HHMMSS.png
tools/xvfb/noir-xvfb stop
```

`noir-xvfb help` lists every command and option.

## Inspecting

| Command | What you get |
| --- | --- |
| `shot [FILE]` | The X server's framebuffer: what a user would see, stacking included |
| `grab WINDOW [FILE]` | One shell surface as rendered by Qt Quick, alpha preserved. Use it for popups, whose translucent margins are black on screen without a compositor |
| `windows` | Shell surfaces: X11 id, geometry, visible/active, title |
| `tree [WINDOW] [--all]` | Qt Quick item tree: type, `#id`, screen rectangle, text-like properties, `[focus]` / `[hidden]` / `[opacity]` flags |
| `eval 'EXPR'` | Result of a QML/JavaScript expression evaluated in `Main.qml` |
| `status` | `_NET_WORKAREA`, `_NET_ACTIVE_WINDOW`, stacking order, managed windows |
| `logs [shell\|wm\|xvfb\|dbus\|clients\|vnc] [-f]` | Per-process logs from `build/xvfb/` |

`WINDOW` is a case-insensitive title substring (`Taskbar`, `Desktop`,
`Applications`, `Notifications`) or an X11 window id.

`eval` sees the `shell` object (`SessionShell.qml` / `WindowedShell.qml`) and
every singleton of the `Noir.Shell` module:

```bash
noir-xvfb eval 'shell.tasks.count'
noir-xvfb eval 'shell.startMenuOpen = true'
noir-xvfb eval 'Theme.reducedMotion = true'      # stable screenshots
noir-xvfb eval 'NotificationServer.post("Hi", "posted from eval")'
```

## Driving

```bash
noir-xvfb click 26 696                      # screen coordinates
noir-xvfb click '@toolTip="Applications"'   # centre of the first visible item whose tree line matches
noir-xvfb click 500 300 3                   # right click -> desktop context menu
noir-xvfb key alt+F1                        # also: super, ctrl+shift+Tab, Escape, Return ...
noir-xvfb type "term"
noir-xvfb drag 1090 60 800 200              # move a desktop widget
noir-xvfb notify "Build finished" "0 warnings"
```

`@PATTERN` is an extended regex matched against the lines printed by
`noir-xvfb tree`, so anything visible there (`#id`, type name, `text="..."`)
can be used to address an item.

Input goes through `noir-xinject` (XTEST), which also works standalone on any
display: `DISPLAY=:1 build/tools/xvfb/noir-xinject key super`.

## Edit–build–look loop

```bash
noir-xvfb reload        # cmake --build build --target NoirShell, restart only the shell
noir-xvfb shot
```

The X server, bus, window manager and any spawned clients stay up across a
reload.

## Watching it live

```bash
noir-xvfb vnc                  # x0vncserver on localhost:5999, no password, loopback only
vncviewer localhost::5999
```

## Debugging

```bash
# Backtrace if the shell crashes during startup or later
noir-xvfb start --wrap "gdb -batch -ex run -ex bt --args"

# Attach to the running shell
gdb -p "$(cat build/xvfb/shell.pid)"

# Renderer details in the shell log
QSG_INFO=1 noir-xvfb start && noir-xvfb logs

# Run your own tools against the session
eval "$(noir-xvfb env)"; xprop -root; qdbus6 org.siriusos.NoirShell /Debug
noir-xvfb run xwininfo -root -tree
```

Other window managers can be swapped in with `--wm "openbox"` (or `--no-wm`),
and `--windowed` runs the single-window development shell instead.

## Rendering under Xvfb

Xvfb has no DRI3, so Mesa's hardware Vulkan drivers cannot present to it.
NoirShell detects that and falls back to its OpenGL backend, which the
harness runs on llvmpipe. Pick explicitly with `--backend`:

| Backend | Under Xvfb |
| --- | --- |
| `auto` (default) | OpenGL on llvmpipe; full fidelity |
| `opengl` | Same |
| `software` | Qt Quick's raster renderer; shader effects are skipped, so tinted first-party icons do not draw |
| `vulkan` | Only works with a software Vulkan driver (lavapipe) installed; otherwise the shell exits and `start` prints the log |

By default the harness runs `NoirWM --no-compositor`, so translucent popup
margins show as black in `shot`; use `grab` to see the surface itself.

`noir-xvfb start --compositor` turns NoirWM's Vulkan compositor on. That
needs a software Vulkan driver, since hardware ones cannot present to Xvfb:
install lavapipe (`libvulkan_lvp` on openSUSE), or point `VK_DRIVER_FILES`
at a copy of its ICD manifest. `shot` then shows the composited result,
translucency and shadows included.

## Smoke test

```bash
noir-xvfb smoke          # or: ctest --test-dir build
```

Boots a throwaway `--demo` session in `build/xvfb-smoke/`, checks that the
desktop and taskbar map, that NoirWM applied the taskbar strut, that a D-Bus
notification arrives, that a synthesized click on the start button opens the
start menu, and that a client window gets a frame which `Alt+F10` maximizes
and `Alt+F4` closes. It saves `shots/smoke.png` and tears everything down,
exiting non-zero if any check fails. `noir-xvfb smoke --compositor` runs the
same checks with compositing on.

## Files

```
tools/xvfb/
├── noir-xvfb        the harness (bash)
├── xinject.c        noir-xinject: XTEST pointer/keyboard injector
└── CMakeLists.txt   builds noir-xinject, registers the smoke test with CTest
```

Runtime state (`display`, `bus.addr`, `*.pid`, `*.log`, `shots/`) lives in
`build/xvfb/`; override with `NOIR_XVFB_STATE`, and point at another build
tree with `NOIR_BUILD_DIR`.
