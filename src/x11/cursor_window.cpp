#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

#include "x11/cursor_window.h"

namespace
{
    uint32_t CursorType_to_x11_cursor(CursorType type)
    {
        switch (type)
        {
            case CursorType::Pointer: return XC_left_ptr;
            case CursorType::Hand: return XC_hand2;
            case CursorType::IBeam: return XC_xterm;
            case CursorType::DownArrow: return XC_sb_down_arrow;
        }

        return XC_num_glyphs;
    }
}


XImage* CursorPixel::get_mouse_image(CursorType cursor_type)
{
    XcursorImage* cursor_image = XcursorShapeLoadImage(CursorType_to_x11_cursor(cursor_type), NULL, 0);

    if (!cursor_image)
    {
        return nullptr;
    }

    // the cursor image will have some transparent padding around it, so it'll need to be cropped
    size_t start_x = cursor_image->width - 1;
    size_t start_y = cursor_image->width - 1;
    size_t end_x = 0;
    size_t end_y = 0;

    /*
    Clip off the rows and columns where the alpha channel is 0 
    0 00000 000
    ___________
    0 11111 000
    0 11110 000
    0 00110 000
    0 00011 000
    ___________
    0 00000 000
    */

    for (size_t y = 0; y < cursor_image->height; y++)
    {
        bool row_not_empty = false;
        size_t x = 0;

        for (; x < cursor_image->width; x++)
        {
            uint8_t alpha = cursor_image->pixels[y * cursor_image->width + x] >> 24;

            if(alpha != 0)
            {
                row_not_empty = true;

                break;
            }
        }

        if (row_not_empty)
        {
            if(y < start_y) start_y = y;                
            if(x < start_x) start_x = x;
        }
    }

    for (size_t y = cursor_image->height; y-- != 0;)
    {
        bool row_not_empty = false;
        size_t x = cursor_image->width;

        while (x-- != 0)
        {
            uint8_t alpha = cursor_image->pixels[y * cursor_image->width + x] >> 24;

            if(alpha != 0)
            {
                row_not_empty = true;

                break;
            }
        }

        if (row_not_empty)
        {
            if(y > end_y) end_y = y;        
            if(x > end_x) end_x = x;
        }
    }

    size_t cursor_width = (end_x - start_x) + 1;
    size_t cursor_height = (end_y - start_y) + 1;

    // should use new keyword in C++ but since X11 is a C API...
    uint32_t* pixels = (uint32_t*)malloc(cursor_width * cursor_height * sizeof(uint32_t));

    if(!pixels) return nullptr;

    for(size_t y = 0; y < cursor_height; y++)
    {
        for(size_t x = 0; x < cursor_width; x++)
        {
            pixels[y * cursor_width + x] = cursor_image->pixels[(start_y + y) * cursor_image->width + (start_x + x)];
        }
    }

    XImage* mouse_image = XCreateImage(x11.display, x11.visual_info.visual, 32, ZPixmap, 0, reinterpret_cast<char*>(pixels), cursor_width, cursor_height, 32, 0);

    XcursorImageDestroy(cursor_image);

    return mouse_image;
}

CursorPixel::CursorPixel(X11State& state, CursorPixel::cursor_list cursor_shades)
:
x11(state),
_max_width(0),
_max_height(0)
{
    if(cursor_shades.size() == 0)
    {
        std::cerr << "error: no cursor list provided to CursorPixel()" << std::endl;

        std::exit(EXIT_FAILURE);
    }

    for (CursorType type : cursor_shades)
    {
        XImage* mouse_image = get_mouse_image(type);

        if(!mouse_image)
        {
            std::cerr << "error: could not get mouse cursor image" << std::endl;

            CursorPixel::~CursorPixel();

            std::exit(EXIT_FAILURE);
        }

        if(mouse_image->width > _max_width) _max_width = mouse_image->width;
        if(mouse_image->height > _max_height) _max_height = mouse_image->height;

        _image_shades.push_back(mouse_image);
    }
}

CursorPixel::~CursorPixel()
{
    for (XImage* image : _image_shades)
    {
        XDestroyImage(image);
    }
}


#define CreateColourmap XCreateColormap
#define CWColourmap CWColormap

int CursorOverlayWindow::create_window()
{
    XSetWindowAttributes attrs;

    attrs.colormap = CreateColourmap(x11.display, x11.root_window, x11.visual_info.visual, AllocNone);
    attrs.border_pixel = BlackPixel(x11.display, x11.screen_id);
    attrs.background_pixel = BlackPixel(x11.display, x11.screen_id);
    attrs.override_redirect = true;

    _window = XCreateWindow(x11.display, x11.root_window, x11.monitor_region.x, x11.monitor_region.y, x11.monitor_region.width, x11.monitor_region.height, 0, x11.visual_info.depth, InputOutput, x11.visual_info.visual,
                                    CWColourmap | CWBorderPixel | CWBackPixel | CWOverrideRedirect, &attrs);

    if (_window == None)
    {
        std::cerr << "Could not create X11 window!" << std::endl;

        return EXIT_FAILURE;
    }

    Atom window_type_atom = XInternAtom(x11.display, "_NET_WM_WINDOW_TYPE", false);
    Atom window_type_dock_atom = XInternAtom(x11.display, "_NET_WM_WINDOW_TYPE_DOCK", false);
    
    XChangeProperty(x11.display, _window, window_type_atom, XA_ATOM, 32, PropModeReplace, (unsigned char*)&window_type_dock_atom, 1);

    // pass events (eg mouse, keyboard) to window behind instead of the overlay
    XserverRegion region = XFixesCreateRegion(x11.display, nullptr, 0);

    XFixesSetWindowShapeRegion(x11.display, _window, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(x11.display, region);

    XMapWindow(x11.display, _window);

    _gc = XCreateGC(x11.display, _window, 0, nullptr);

    uint32_t* frame_data = (uint32_t*)malloc(x11.monitor_region.width * x11.monitor_region.height * sizeof(uint32_t));

    if(!frame_data)
    {
        return EXIT_FAILURE;
    }

    _backbuffer = XCreateImage(x11.display, x11.visual_info.visual, 32, ZPixmap, 0, (char*)frame_data, x11.monitor_region.width, x11.monitor_region.height, 32, 0);

    if(!_backbuffer)
    {
        return EXIT_FAILURE;
    }

    XFlush(x11.display);

    return EXIT_SUCCESS;
}

void CursorOverlayWindow::write_frame(const ImageBuffer<uint8_t>& data)
{
    uint32_t* pixels = (uint32_t*)_backbuffer->data;

    for (size_t y = 0; y < data.height; y++)
    {
        for (size_t x = 0; x < data.width; x++)
        {
            XImage* mouse_image = _cursors.get_cursor_image(data.pixels[y * data.width + x]);

            if (mouse_image)
            {
                size_t screen_x = x * _cursors.max_width();
                size_t screen_y = y * _cursors.max_height();
                size_t curs_width = std::min((size_t)_backbuffer->width - screen_x, (size_t)mouse_image->width);
                size_t curs_height = std::min((size_t)_backbuffer->height - screen_y, (size_t)mouse_image->height);

                for (size_t curs_y = 0; curs_y < curs_height; curs_y++)
                {
                    std::memcpy(&pixels[(screen_y + curs_y) * (size_t)_backbuffer->width + screen_x], &(((uint32_t*)mouse_image->data)[curs_y * mouse_image->width]), curs_width * sizeof(uint32_t));
                }
            }
        }
    }
}

void CursorOverlayWindow::swap_buffers()
{
    XPutImage(x11.display, _window, _gc, _backbuffer, 0, 0, 0, 0, x11.monitor_region.width, x11.monitor_region.height);
    std::memset(_backbuffer->data, 0, x11.monitor_region.width * x11.monitor_region.height * sizeof(uint32_t));
}

CursorOverlayWindow::~CursorOverlayWindow()
{
    XDestroyWindow(x11.display, _window);
    XFreeGC(x11.display, _gc);
    XDestroyImage(_backbuffer);
}
