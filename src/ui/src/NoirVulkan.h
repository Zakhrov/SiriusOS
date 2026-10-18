#pragma once

class QGuiApplication;

namespace Noir {

// EWMH: whether a compositing manager owns _NET_WM_CM_S0, i.e. whether
// translucent windows will be blended.
bool compositingManagerRunning();

// Every Noir program renders with Qt Quick on Vulkan only: a Vulkan 1.0
// instance of its own, on the device the rest of the stack uses (see
// NoirVulkanDevice.h), handed to each window as it is created. Call after
// constructing the application and before creating any window. `who` prefixes
// the log lines. `composited` says translucent surfaces are wanted;
// `*translucent` says whether the chosen device can provide them. Returns
// false when there is no Vulkan device that can present to the X server.
bool setUpVulkan(QGuiApplication *app, const char *who, bool composited, bool *translucent);

} // namespace Noir
