#include <iostream>

#include "video_player.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

#define AV_PIX_FMT_GREY8 AV_PIX_FMT_GRAY8

std::optional<std::string> VideoPlayer::open_video(const char* video_filename, size_t window_width, size_t window_height)
{
    int res = avformat_open_input(&_format_context, video_filename, nullptr, nullptr);

    if (res < 0)
    {
        char err_str[AV_ERROR_MAX_STRING_SIZE];
        
        av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, res);

        return err_str;
    }

    if (avformat_find_stream_info(_format_context, nullptr) < 0)
    {
        return "failed to find stream information";
    }

    _video_stream_index = av_find_best_stream(_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (_video_stream_index < 0)
    {
        return "no video stream found";
    }

    const AVStream* video_stream = _format_context->streams[_video_stream_index];
    const AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);

    if (!codec)
    {
        return "A decoder for this video codec was not found";
    }

    _fps = video_stream->r_frame_rate.num / video_stream->r_frame_rate.den;

    _codec_context = avcodec_alloc_context3(codec);
    
    if(!_codec_context || avcodec_parameters_to_context(_codec_context, video_stream->codecpar) < 0)
    {
        return "Could not create a video codec context";
    }

    if (avcodec_open2(_codec_context, codec, nullptr) < 0)
    {
        return "Failed to open codec";
    }

    _packet = av_packet_alloc();
    _frame = av_frame_alloc();

    _resize_width = window_width;
    _resize_height = window_height;

    _sws_context = sws_getContext(_codec_context->width, _codec_context->height, _codec_context->pix_fmt,
                                    _resize_width, _resize_height, AV_PIX_FMT_GREY8,
                                    SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    return {};
}

bool VideoPlayer::get_next_frame(uint8_t* buffer)
{
    do
    {
        if (av_read_frame(_format_context, _packet) < 0)
        {
            return false;
        }
    }
    while(_packet->stream_index != _video_stream_index);

    int response = avcodec_send_packet(_codec_context, _packet);

    if (response < 0)
    {
        std::cerr << "Error sending a packet for decoding" << std::endl;

        av_packet_unref(_packet);

        return false;
    }

    response = avcodec_receive_frame(_codec_context, _frame);

    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
    {
        av_packet_unref(_packet);

        return false;
    }
    else if (response < 0)
    {
        std::cerr << "Error during decoding" << std::endl;

        av_packet_unref(_packet);

        return false;
    }

    uint8_t* sws_data[AV_NUM_DATA_POINTERS] = { buffer };
    int sws_linesize[AV_NUM_DATA_POINTERS] = { (int)_resize_width };

    sws_scale(_sws_context, _frame->data, _frame->linesize, 0, _codec_context->height, sws_data, sws_linesize);

    av_packet_unref(_packet);

    return true;
}

VideoPlayer::~VideoPlayer()
{
    avformat_close_input(&_format_context);
    avformat_free_context(_format_context);
    avcodec_free_context(&_codec_context);
    av_frame_free(&_frame);
    sws_freeContext(_sws_context);
    av_packet_free(&_packet);
}
