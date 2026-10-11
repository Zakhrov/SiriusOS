# SiriusOS

SiriusOS is a modern, sleek, and cyberpunk-inspired operating system designed to provide a visually stunning and immersive user experience. The design emphasizes sharp edges, 3D effects, and a glass-like aesthetic, creating a unique and futuristic interface.

## Features (Tentatively planned)

- **Noir Desktop Environment**: A custom desktop environment with a black and silver/white color scheme, featuring transparency effects and 3D animations, all powered by Vulkan
- **X11 Only**: Currently supports only the X11 windowing system, with **no** plans to support Wayland in the future
- **Vulkan only graphics stack**: Ships with only Vulkan drivers and libraries, with OpenGL support provided via the Zink Mesa driver

## Repository Contents (As of now)

- **Noir Desktop Environment Window Manager**: A from-scratch X11 window manager with a Vulkan based compositor
- **Noir Shell**: The Qt 6 / QML desktop shell (desktop, widgets, taskbar, start menu, system tray and notifications), rendered with Vulkan. See [src/shell/README.md](src/shell/README.md)
