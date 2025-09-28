#pragma once

#include <X11/Xlib.h>

#include "NoirWM.h"

void handle_map_request(NoirWM* wm, XMapRequestEvent* event);
void handle_configure_request(NoirWM* wm, XConfigureRequestEvent* event);