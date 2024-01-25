#pragma once

#include <chrono>
#include <thread>
#include <cstring>

template <typename T>
T round_up_div(T a, T b)
{
    return a % b == 0 ? a / b : (a / b) + 1;
} 

struct RectangleRegion
{
    size_t x, y;
    size_t width, height;
};

template <typename T>
struct ImageBuffer
{
    T* const pixels;
    size_t width, height;

    ImageBuffer(T* buff, size_t w, size_t h)
    :
    pixels(buff),
    width(w),
    height(h)
    {
    }

    size_t size() const
    {
        return width * height * sizeof(T);
    }

    void clear()
    {
        std::memset(pixels, 0, size());
    }
};

class FrameCounter
{
private:
    size_t _frame_index;
    std::chrono::time_point<std::chrono::steady_clock> _previous_time;
    std::chrono::milliseconds _total_time;
    std::chrono::milliseconds _frame_time;
    std::chrono::milliseconds _lost_time;
    const std::chrono::milliseconds _min_frame_time;

public:
    FrameCounter(size_t max_fps)
    :
    _frame_index((size_t)-1),
    _total_time(0),
    _frame_time(0),
    _lost_time(0),
    _min_frame_time(max_fps ? 1000 / max_fps : 0)
    {
    }

    void begin_frame()
    {
        _previous_time = std::chrono::steady_clock::now();
        _frame_index++;
    }

    void end_frame()
    {
        _frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _previous_time);

        if(_min_frame_time.count())
        {
            if (_frame_time < _min_frame_time)
            {
                std::chrono::milliseconds offset = _min_frame_time - _frame_time;

                if(offset > _lost_time)
                {
                    std::this_thread::sleep_for(offset);

                    _frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _previous_time);
                }
                else
                {
                    _lost_time -= offset;
                }
            }
            else
            {
                _lost_time += _frame_time - _min_frame_time;
            }
        }

        _total_time += _frame_time;
    }

    size_t frame_index() const
    {
        return _frame_index;
    }

    size_t fps()
    {
        return 1000 / _frame_time.count();
    }

    size_t average_fps()
    {
        return 1000 / (_total_time.count() / (_frame_index + 1));
    }

    size_t dropped_frames()
    {
        if(!_min_frame_time.count()) return 0;

        size_t fps = 1000 / _frame_time.count();
        size_t max_fps = 1000 / _min_frame_time.count();

        return fps < max_fps ? max_fps - fps : 0;
    }

    size_t average_dropped_frames()
    {
        if(!_min_frame_time.count()) return 0;

        size_t avg_fps = average_fps();
        size_t max_fps = 1000 / _min_frame_time.count();

        return avg_fps < max_fps ? max_fps - avg_fps : 0;
    }
};

enum class CursorType
{
    Pointer,
    Hand,
    IBeam,
    DownArrow
};
