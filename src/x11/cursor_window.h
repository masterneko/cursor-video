#pragma once

#include <vector>
#include <cstdint>

#include "x11/state.h"

class CursorPixel
{
private:
    X11State& x11;
    std::vector<XImage*> _image_shades;
    size_t _max_width, _max_height;

    XImage* get_mouse_image(CursorType cursor_type);

public:
    using cursor_list = std::initializer_list<CursorType>;

    CursorPixel(X11State& state, cursor_list cursor_shades);

    // Translates value out of 255 into an index
    // which it uses to find a cursor in the cursors array.
    // White shades (around 255ish) return null images 
    XImage* get_cursor_image(uint8_t shade)
    {
        size_t index = (size_t)shade * (_image_shades.size() + 1) / 256;

        return index >= _image_shades.size() ? nullptr : _image_shades[index];
    }

    size_t max_width() const { return _max_width; }
    size_t max_height() const { return _max_height; }

    ~CursorPixel();
};

class CursorOverlayWindow
{
private:
    X11State& x11;
    Window _window;
    GC _gc;
    CursorPixel _cursors;
    XImage* _backbuffer;

public:
    CursorOverlayWindow(X11State& state, CursorPixel::cursor_list cursors)
    :
    x11(state),
    _window(None),
    _cursors(x11, cursors),
    _backbuffer(nullptr)
    {
    }

    int create_window();

    void write_frame(const ImageBuffer<uint8_t>& data);
    void swap_buffers();

    size_t get_width() const
    {
        return round_up_div((size_t)_backbuffer->width, _cursors.max_width());
    }

    size_t get_height() const
    {
        return round_up_div((size_t)_backbuffer->height, _cursors.max_height());
    }

    ~CursorOverlayWindow();
};