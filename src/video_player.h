#pragma once

#include <string>
#include <optional>
#include <cstdint>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

class VideoPlayer
{
private:
    AVFormatContext* _format_context;
    AVCodecContext* _codec_context;
    AVFrame* _frame;
    AVPacket* _packet;
    struct SwsContext* _sws_context;
    int _video_stream_index;
    size_t _resize_width, _resize_height;
    size_t _fps;

public:
    VideoPlayer()
    :
    _format_context(nullptr),
    _codec_context(nullptr),
    _frame(nullptr),
    _packet(nullptr),
    _sws_context(nullptr),
    _video_stream_index(0),
    _resize_width(0),
    _resize_height(0),
    _fps(0)
    {
    }
    
    std::optional<std::string> open_video(const char* video_filename, size_t window_width, size_t window_height);

    size_t framerate() const
    {
        return _fps;
    }

    // copies frame data into buffer
    // returns true on success
    bool get_next_frame(uint8_t* buffer);

    ~VideoPlayer();
};
