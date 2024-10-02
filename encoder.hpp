#ifndef ENCODER_HPP
#define ENCODER_HPP

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

#include "threadsafequeue.hpp"
#include <thread>

#include <iostream>

static AVPixelFormat hw_pix_fmt_encoder = AV_PIX_FMT_CUDA;

class Encoder
{
private:
    AVCodecContext *codec_ctx = NULL;
    SwsContext* scaler_ctx = NULL;
    ThreadSafeQueue<uint8_t*> *frame_queue;
    ThreadSafeQueue<AVPacket*> *encode_queue;
    /* data */
public:
    Encoder(ThreadSafeQueue<uint8_t*> *frame_queue, ThreadSafeQueue<AVPacket*> *encode_queue);
    ~Encoder();
    static AVPixelFormat get_pix_fmt_hw(AVCodecContext* ctx, const AVPixelFormat* fmt);
    void encode();
    std::thread start_encoder_thread();
    std::thread start_send_encoded_to_decode();
    void send_encode_to_decode();
};


#endif