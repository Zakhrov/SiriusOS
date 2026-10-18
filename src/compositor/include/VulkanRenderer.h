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
// Rebuild the swapchain for the target's current size.
void renderer_resize(VulkanRenderer* renderer);

RendererTexture* renderer_texture_create(VulkanRenderer* renderer, int width, int height);
void renderer_texture_destroy(VulkanRenderer* renderer, RendererTexture* texture);
// CPU-side pixel store: width * height BGRA pixels, rows tightly packed.
uint8_t* renderer_texture_pixels(RendererTexture* texture);
// Schedule a region of the pixel store for upload at the start of the next frame.
void renderer_texture_update(VulkanRenderer* renderer, RendererTexture* texture,
                             int x, int y, int width, int height);

// Queue before begin_frame; a compute dispatch builds each mesh before drawing.
// Kinds: 1 = wobble, 2 = lamp (progress 0 open .. 1 minimized), 3 = shatter.
int renderer_prepare_effect(VulkanRenderer* renderer, RendererTexture* texture,
                            int x, int y, int width, int height, int hasAlpha,
                            int kind, float progress, float seed,
                            const float target[4], const float motion[4]);
void renderer_draw_effect(VulkanRenderer* renderer, RendererTexture* texture);
int renderer_effects_supported(const VulkanRenderer* renderer);

// A frame is begin, any number of draws (painted in call order), end.
// begin returns 0 when no image could be acquired; skip the frame then.
// `offscreen` asks for a frame that can blur (see renderer_draw_blur).
int renderer_begin_frame(VulkanRenderer* renderer, int offscreen);
// Whether blur behind windows is available on this device.
int renderer_blur_supported(const VulkanRenderer* renderer);
// Frosted glass: blurs what has been drawn so far inside the rectangle and
// paints it back there, for a translucent window drawn next to sit on. Only
// in offscreen frames.
void renderer_draw_blur(VulkanRenderer* renderer, int x, int y, int width, int height);
void renderer_draw_texture(VulkanRenderer* renderer, RendererTexture* texture,
                           int x, int y, int width, int height, int hasAlpha, float opacity);
// Soft shadow around the rectangle (x, y, width, height), `radius` pixels wide.
void renderer_draw_shadow(VulkanRenderer* renderer, int x, int y, int width, int height,
                          int radius, float strength);
void renderer_end_frame(VulkanRenderer* renderer);
