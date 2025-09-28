#include "NoirWM.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv) {
    NoirWM* wm = NoirWM_create();
    if (!wm) {
        fprintf(stderr, "Failed to create NoirWM instance\n");
        return EXIT_FAILURE;
    }
    NoirWM_run(wm);
    NoirWM_destroy(wm);
    return EXIT_SUCCESS;
}