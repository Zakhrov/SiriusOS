#pragma once
#include <vulkan/vulkan.h>

// Records the basic Noir desktop (wallpaper, icons, widget, window, taskbar)
// into a command buffer. Must be called inside an active render pass.
void recordDesktopScene(VkCommandBuffer commandBuffer, VkExtent2D extent);
