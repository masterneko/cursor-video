#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>

#include "x11/state.h"
#include "x11/cursor_window.h"
#include "video_player.h"

int main(int argc, const char* const argv[])
{
    if(argc < 2)
    {
        std::cout << "Please provide a filename to the video which you intend on playing" << std::endl;

        return EXIT_FAILURE;
    }

    const char* video_filename = argv[1];

    X11State x11;
    CursorOverlayWindow window(x11, {
        CursorType::Pointer,
        CursorType::Hand,
        CursorType::DownArrow,
        CursorType::IBeam
    });

    int err = window.create_window();

    if (err)
    {
        return err;
    }

    VideoPlayer video_player;

    if (auto err = video_player.open_video(video_filename, window.get_width(), window.get_height()))
    {
        std::cerr << "error: " << video_filename << ": " << *err << std::endl;

        return EXIT_FAILURE;
    }

    std::cout << "Mouse display resolution: " << window.get_width() << "x" << window.get_height()
              << " (" << window.get_width() * window.get_height() << " pixels)" << std::endl;

    uint8_t frame_buffer[window.get_width() * window.get_height()];
    ImageBuffer<uint8_t> frame(frame_buffer, window.get_width(), window.get_height());
    FrameCounter frame_counter(video_player.framerate());

    while(true)
    {
        frame_counter.begin_frame();

        if (!video_player.get_next_frame(frame.pixels)) break;

        window.write_frame(frame);
        window.swap_buffers();

        frame_counter.end_frame();

        if(frame_counter.frame_index() % 10 == 0)
        {
            size_t dropped_frames = frame_counter.average_dropped_frames();

            if (dropped_frames > 1)
            {
                std::cout << dropped_frames << " dropped frames" << std::endl;
            }
            else if (dropped_frames != 0)
            {
                std::cout << dropped_frames << " dropped frame" << std::endl;
            }
        }
    }

    return EXIT_SUCCESS;
}
