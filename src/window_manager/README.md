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
