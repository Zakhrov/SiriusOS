#include "VulkanRenderer.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    VulkanRenderer* renderer = createTestVulkanRenderer(1280, 720);
    if (!renderer) {
        fprintf(stderr, "Failed to create VulkanRenderer\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 600; i++) { // ~10 seconds at 60Hz (FIFO)
        drawVulkanRendererFrame(renderer);
    }

    destroyVulkanRenderer(renderer);
    return EXIT_SUCCESS;
}