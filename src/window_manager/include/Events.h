#pragma once

#include <X11/Xlib.h>
#include "NoirWM.h"

void handle_event(NoirWM* wm, XEvent* event);