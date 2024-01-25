#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <iostream>
#include <cstdlib>
#include <cstdint>

#include "x11/state.h"

#define TrueColour TrueColor


X11State::X11State()
{
    display = XOpenDisplay(nullptr);

    if (!display)
    {
        std::cerr << "X11 display initialisation failed!" << std::endl;

        std::exit(EXIT_FAILURE);
    }

    screen_id = DefaultScreen(display);
    root_window = DefaultRootWindow(display);

    if(!XMatchVisualInfo(display, screen_id, 32, TrueColour, &visual_info))
    {
        std::cerr << "XMatchVisualInfo failed" << std::endl;

        std::exit(EXIT_FAILURE);
    }

    {
        int mouse_x = 0, mouse_y = 0;
        Window root_return, child_return;
        int win_x_return, win_y_return;
        uint32_t mask_return;

        if (!XQueryPointer(display, root_window, &root_return, &child_return, &mouse_x, &mouse_y, &win_x_return, &win_y_return, &mask_return))
        {
            std::cerr << "XQueryPointer failed" << std::endl;

            std::exit(EXIT_FAILURE);
        }

        XRRScreenResources* screen_res = XRRGetScreenResourcesCurrent(display, root_window);
        XRRCrtcInfo* active_monitor_info = nullptr;

        for (size_t i = 0; i < screen_res->ncrtc; i++)
        {
            XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(display, screen_res, screen_res->crtcs[i]);

            if(crtc_info &&
            mouse_x >= crtc_info->x && mouse_x < crtc_info->x + crtc_info->width &&
            mouse_y >= crtc_info->y && mouse_y < crtc_info->y + crtc_info->height)
            {
                active_monitor_info = crtc_info;

                break;
            }

            XRRFreeCrtcInfo(crtc_info);
        }

        if(!active_monitor_info)
        {
            std::cerr << "Could not find active monitor" << std::endl;

            std::exit(EXIT_FAILURE);
        }

        printf("Monitor: x: %d, y: %d, width: %u, height: %u\n", active_monitor_info->x, active_monitor_info->y, active_monitor_info->width, active_monitor_info->height);

        monitor_region = {
            static_cast<size_t>(active_monitor_info->x),
            static_cast<size_t>(active_monitor_info->y),
            active_monitor_info->width,
            active_monitor_info->height
        };

        XRRFreeCrtcInfo(active_monitor_info);
        XRRFreeScreenResources(screen_res);
    }
}

X11State::~X11State()
{
    XCloseDisplay(display);
}
