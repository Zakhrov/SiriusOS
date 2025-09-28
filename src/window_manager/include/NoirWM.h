#pragma once

#include <X11/Xlib.h>
#include <VulkanRenderer.h>

typedef struct {
    Display* display;
    Window root;
    VulkanRenderer* renderer;
} NoirWM;
NoirWM* NoirWM_create();
void NoirWM_destroy(NoirWM* wm);
void NoirWM_run(NoirWM* wm);