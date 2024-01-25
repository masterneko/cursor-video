#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "misc.h"

struct X11State
{
    Display* display;
    int screen_id;
    Window root_window;
    XVisualInfo visual_info;
    RectangleRegion monitor_region;

    X11State();
    X11State(const X11State&) = delete;

    ~X11State();
};
