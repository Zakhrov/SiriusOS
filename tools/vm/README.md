# QEMU VM (`noir-vm`)

Builds an **openSUSE Leap 15.6** guest that boots straight into the Noir
Desktop Environment, the way an installed system would:

```
QEMU/KVM -> Leap 15.6 -> LightDM (autologin) -> noir-session -> NoirWM (+ compositor) + NoirShell
```

Everything runs unprivileged on the host: KVM through `/dev/kvm`, user-mode
networking, and state under `build/vm/`.

## Requirements

Host: `qemu-system-x86_64` with access to `/dev/kvm` and, for Venus, QEMU
9.2 or later built against a Venus-enabled virglrenderer, plus a DRM render
node (`/dev/dri/renderD*`); `qemu-img`, `curl`, `ssh`, `python3`, and
`xorriso` or `mkisofs`. A VNC viewer (`vncviewer`,
`remote-viewer`) to watch the guest.

The Noir binaries are built on the host and copied into the guest, so the
host must be ABI compatible with Leap 15.6 (in practice: build on Leap 15.6).

## Quick start

```bash
tools/vm/noir-vm up        # ~10 min the first time: download, provision, build, deploy
tools/vm/noir-vm view      # watch and use the desktop over VNC
tools/vm/noir-vm stop
```

`up` is the sum of these steps, each of which can be run on its own:

| Step | What it does |
| --- | --- |
| `fetch` | Downloads the Leap 15.6 Minimal-VM cloud image (about 300 MB) into `build/vm/cache/` and verifies its SHA-256 |
| `create` | Creates a copy-on-write disk on top of it, an SSH key pair and a cloud-init seed ISO |
| `start [--gui]` | Boots the guest headless with VNC on `127.0.0.1:5926` and SSH on port 2226. `--gui` also opens a QEMU window |
| `provision` | Waits while cloud-init creates the `noir` user, installs the packages below and reboots into `kernel-default` |
| `deploy` | Builds the tree, `cmake --install`s it into the guest under `/usr`, restarts LightDM and waits for the shell |

Later boots (`noir-vm start`) go straight to the desktop. After changing
code, `noir-vm deploy` is the whole edit-to-screen loop.

Guest login is `noir` / `noir`, with passwordless `sudo`. SSH is key-only and
forwarded on loopback.

## What is in the guest

- **Session**: [`noir-session`](../../src/session/noir-session) and
  `/usr/share/xsessions/noir.desktop`, installed by `cmake --install`.
  LightDM logs `noir` in automatically and starts that session.
- **Packages** (see `GUEST_PACKAGES` in the script): X.Org server and
  libinput, LightDM, Mesa with the lavapipe software Vulkan driver (and its
  Venus driver where the distribution packages it), the Qt 6
  runtime the shell and the Control Center link against, the helpers and
  services the Control Center uses (`setxkbmap`, `pactl`, UPower,
  `xdg-utils`), fonts and icon themes, and a few clients (`xterm`, `xclock`,
  `xeyes`, `notify-send`).
- **`/etc/noir/session.conf`**: starts the shell with `--debug-interface` so
  the host can inspect it.

### Rendering

The Noir stack renders with Vulkan only. How the guest gets it depends on
the host QEMU (`NOIR_VM_VENUS`, default `auto`):

| | Venus available | Otherwise (QEMU 8.2 on Leap 15.6, for one) |
| --- | --- | --- |
| Adapter | `virtio-vga-gl` with `venus=true,blob=true` | Plain `virtio-vga` framebuffer, no 3D |
| Guest Vulkan | Mesa's Venus driver, executed by the host GPU's Vulkan driver | lavapipe, on the guest CPU |
| X server | glamor (DRI3) on the virtio-gpu | Unaccelerated |
| NoirWM compositor, NoirShell | Venus | lavapipe; shell surfaces stay opaque, because software Vulkan cannot present to ARGB windows |

`auto` uses Venus when `qemu-system-x86_64 -device virtio-vga-gl,help` lists
a `venus` property and the host has a render node; `start` says which one it
picked and `noir-vm status` shows the guest's Vulkan devices. The guest also
needs Mesa's Venus driver, which is installed when the distribution ships it
(Leap 15.6 does not), so with Venus on the host the shell may still end up on
lavapipe. `NOIR_VM_VENUS=1` insists on Venus, `0` never uses it.

## Looking and poking

```bash
noir-vm status                       # VM, display manager, session, renderer, surfaces
noir-vm shot [FILE]                  # screenshot straight from QEMU
noir-vm ssh                          # shell in the guest
noir-vm logs shell|wm|lightdm|xorg|cloud-init

noir-vm spawn xterm                  # start a client in the session
noir-vm session wmctrl -l            # run a command against the session's X display and bus
noir-vm session notify-send "Hi" "from the host"

noir-vm click 26 776                 # synthesize input (noir-xinject inside the guest)
noir-vm key alt+F1
noir-vm type "xterm"

noir-vm eval 'shell.tasks.count'     # NoirShell debug interface
noir-vm tree Taskbar
noir-vm windows
```

`shot` asks QEMU for its framebuffer and, when the guest uses a 3D scanout
that QEMU cannot dump, grabs the X screen from inside the guest instead.

## Settings

| Variable | Default | |
| --- | --- | --- |
| `NOIR_VM_RESOLUTION` | `1280x800` | Guest screen size |
| `NOIR_VM_MEMORY` | `4096` | MiB |
| `NOIR_VM_CPUS` | `4` | |
| `NOIR_VM_DISK_SIZE` | `12G` | Virtual size of the copy-on-write disk |
| `NOIR_VM_SSH_PORT` | `2226` | Host loopback port forwarded to the guest's sshd |
| `NOIR_VM_VNC_DISPLAY` | `26` | VNC on TCP port 5900 + N, loopback only, no password |
| `NOIR_VM_VENUS` | `auto` | `1`: virtio-gpu Venus or fail; `0`: lavapipe only |
| `NOIR_VM_HOSTMEM` | `4G` | Host memory window for Venus blob resources |
| `NOIR_VM_DIR` | `build/vm` | Disk, seed, key, logs, screenshots |

Change the session inside the guest by editing `/etc/noir/session.conf`
(`NOIR_WM_ARGS`, `NOIR_SHELL_ARGS`, or any `export`) and running
`sudo systemctl restart display-manager`.

## Troubleshooting

- **Nothing on screen yet**: `build/vm/console.log` is the guest's serial
  console; `noir-vm logs cloud-init` shows first-boot progress.
- **Session does not start**: `noir-vm logs lightdm`, `noir-vm logs xorg`,
  then `noir-vm logs shell` / `wm`.
- **Start over**: `noir-vm destroy` deletes the disk but keeps the downloaded
  base image, so the next `up` only repeats provisioning.

## Limitations

- The compositor runs on the guest CPU and reads window contents back from
  the X server, so an animating desktop keeps NoirWM at roughly one core.
  Turning on *Reduce Motion* in the desktop's context menu quiets it.
- With Venus, the guest's X server (xorg-server 21.1.11, glamor) may abort when a
  depth-32 window exactly the size of the screen is mapped while compositing.
  NoirShell keeps its desktop surface opaque, and framed application windows
  are not affected, but a frameless full-screen ARGB client would hit it.
- The VDD's window animations are not implemented.
- The guest has no sound card or sound server, and networking is wicked
  rather than NetworkManager, so the Control Center's Sound page reports no
  sound server and its Network page lists the interfaces read-only. Display,
  Input Devices, Power (screen blanking, session actions), Graphics and the
  rest work, and the virtio-vga adapter offers many display modes to try.
