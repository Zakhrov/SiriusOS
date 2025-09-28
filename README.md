# SiriusOS Noir Desktop Environment Visual Design Document (VDD)

The SiriusOS Visual Design Document (VDD) outlines the visual and user interface design principles for the SiriusOS Noir Desktop Environment.

## Color Scheme

The primary color scheme for SiriusOS is black with silver/white highlights, with transparency effects that give a glass-like appearance to windows and UI elements.
This color scheme is intended to convey a sleek and cyberpunk-like aesthetic.

## Layout

- Classic layout with a taskbar at the bottom of the screen, desktop widgets, and sharp-edged corners
- 3D morphing of window and UI elements with smooth animations
- Sharp edges on window and UI elements, avoiding rounded corners
- Menu bars and context menus should have a glass-like transparency effect
- Task bar should have a sleek black glass appearance with silver/white icons and text
- Desktop widgets should be either rectangular or polygonal with sharp edges
- Window controls are in the top right corner, with controls for minimize, maximize, full-screen and close
- Window title bars must have left-aligned window titles, with window icons to the left of the title
- System tray and notification area are in the bottom right corner of the taskbar
- Desktops/Workspaces can be switched using a 3D cube effect
  
## Icons

- 3D rendered icons with sharp edges
- Iconography must be in silver with a black background, 3rd party icons should be untouched

## 3D Effects

- Windows should wobble when moved
- Minimize and maximize animations should have a magic-lamp effect
- Close window animation should have a shattering glass effect
- Task switching should have a 3D flip carousel effect
- Context menus should have a 3D animation that follows their behavior - drop-downs, slide outs, etc.
- Desktop widgets should have a 3D floating effect with smooth drop shadows
- Buttons/Menu items should have a 3D press effect with a slight depression when clicked
- Scroll-bars should have a 3D groove effect with a smooth sliding animation
- Tooltips should have a 3D pop-up effect with a slight bounce animation
- Dialog boxes should have a 3D expansion/explosion effect from the center of the screen
- Notifications should 3D slide in from the bottom right corner and slide out when dismissed
- Progress bars should have a 3D fill effect with smooth animations and percentage indicators
- Mouse cursors should have 3D animations when hovering over interactive elements, and when showing a busy/waiting state
- Mouse cursors should morph between different states with smooth 3D animations
- Startup and shutdown sequences should have 3D animations that reflect the overall design aesthetic
  
## Audio Cues and Effects

- Window actions (open, close, minimize, maximize) should have distinct audio cues that match the 3D animations
- Notifications should have a subtle audio alert that complements the visual notification
- Desktop widgets should have a gentle audio cue when interacted with, enhancing the 3D floating effect
- Click interactions should have an augmented click sound with a slight echo to match the 3D press effect
- Startup and shutdown sequences should have a brief audio theme that reflects the overall design aesthetic

## Accessibility

- All interactions should be accessible via keyboard-only interaction/navigation
- Interactive elements should invert colors when hovered/moused over, or when focused via keyboard navigation
- 3D effects and animations should be optimized for high refresh-rate and variable refresh-rate displays
- Colors should adapt for both HDR and SDR displays
- Stereoscopic rendering mode should be supported for VR/AR headsets

## Implementation Notes

- All first-party icons must be in SVG format
- 3D shaders should be compiled for use with Vulkan SPIR-V
- 3D draw calls should be batched and asynchronous where possible
- Animations should be implemented as Vulkan compute shaders for GPU acceleration
- The desktop environment should be built in C++ using the Qt framework, with QML for UI design
- Target the X11 display server for window management, with C++ implementation
- Use separate Vulkan compute and graphics queues wherever possible
- Leverage Zink Mesa driver for legacy OpenGL application compatibility
