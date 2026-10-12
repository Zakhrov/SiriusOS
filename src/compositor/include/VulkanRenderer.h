#pragma once
#include <X11/Xlib.h>
#include <stdint.h>

// Vulkan side of the compositor: owns the device, the swapchain on the
// output window and one texture per composited window. Knows nothing about
// X11 window management; Compositor.c feeds it pixels and draw calls.
typedef struct VulkanRenderer VulkanRenderer;
typedef struct RendererTexture RendererTexture;

// Returns NULL (after printing why) when Vulkan cannot present to `target`.
VulkanRenderer* renderer_create(Display* display, Window target);
void renderer_destroy(VulkanRenderer* renderer);
const char* renderer_device_name(const VulkanRenderer* renderer);

RendererTexture* renderer_texture_create(VulkanRenderer* renderer, int width, int height);
void renderer_texture_destroy(VulkanRenderer* renderer, RendererTexture* texture);
// CPU-side pixel store: width * height BGRA pixels, rows tightly packed.
uint8_t* renderer_texture_pixels(RendererTexture* texture);
// Schedule a region of the pixel store for upload at the start of the next frame.
void renderer_texture_update(VulkanRenderer* renderer, RendererTexture* texture,
                             int x, int y, int width, int height);

// A frame is begin, any number of draws (painted in call order), end.
// begin returns 0 when no image could be acquired; skip the frame then.
int renderer_begin_frame(VulkanRenderer* renderer);
void renderer_draw_texture(VulkanRenderer* renderer, RendererTexture* texture,
                           int x, int y, int width, int height, int hasAlpha, float opacity);
// Soft shadow around the rectangle (x, y, width, height), `radius` pixels wide.
void renderer_draw_shadow(VulkanRenderer* renderer, int x, int y, int width, int height,
                          int radius, float strength);
void renderer_end_frame(VulkanRenderer* renderer);
