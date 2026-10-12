#include "NoirWM.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    int useCompositor = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-compositor") == 0) {
            useCompositor = 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [--no-compositor]\n"
                   "  --no-compositor  manage windows without the Vulkan compositor\n"
                   "The compositor is also skipped, with a message, when Vulkan cannot\n"
                   "present to the X server.\n", argv[0]);
            return EXIT_SUCCESS;
        }
    }

    NoirWM* wm = NoirWM_create(useCompositor);
    if (!wm) {
        fprintf(stderr, "Failed to create NoirWM instance\n");
        return EXIT_FAILURE;
    }
    NoirWM_run(wm);
    NoirWM_destroy(wm);
    return EXIT_SUCCESS;
}
