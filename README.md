# SiriusOS

SiriusOS is a modern, sleek, and cyberpunk-inspired operating system designed to provide a visually stunning and immersive user experience. The design emphasizes sharp edges, 3D effects, and a glass-like aesthetic, creating a unique and futuristic interface.

## Features (Tentatively planned)

- **Noir Desktop Environment**: A custom desktop environment with a black and silver/white color scheme, featuring transparency effects and 3D animations, all powered by Vulkan
- **X11 Only**: Currently supports only the X11 windowing system, with **no** plans to support Wayland in the future
- **Vulkan only graphics stack**: Ships with only Vulkan drivers and libraries, with OpenGL support provided via the Zink Mesa driver

## Repository Contents (As of now)

- **Noir Desktop Environment Window Manager**: A from-scratch X11 window manager with a Vulkan based compositor
- **Noir Shell**: The Qt 6 / QML desktop shell (desktop, widgets, taskbar, start menu, system tray and notifications), rendered with Vulkan. See [src/shell/README.md](src/shell/README.md)
- **Noir Control Center**: System settings in the style of KDE's System Settings: appearance, notifications, default applications, displays, sound, input devices, power, graphics, network, Bluetooth, date and time, and system information. See [src/control_center/README.md](src/control_center/README.md)
- **Xvfb harness**: Runs the window manager and shell headlessly for inspection, debugging and smoke tests. See [tools/xvfb/README.md](tools/xvfb/README.md)
- **Try it yourself**: `tools/desktop/noir-desktop start` opens the whole desktop in a window with your real GPU (direct DRI3 presentation, the compositor's translucent glass), and `install-session` adds the development build to the login screen for bare metal. See [tools/desktop/README.md](tools/desktop/README.md)
- **Session and VM**: `noir-session` starts the desktop from a display manager, and a QEMU harness, the current target, boots it on openSUSE Leap 15.6 (Vulkan through virtio-gpu Venus when QEMU supports it, lavapipe otherwise). See [tools/vm/README.md](tools/vm/README.md)
