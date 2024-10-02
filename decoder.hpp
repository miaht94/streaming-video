#ifndef DECODER_HPP
#define DECODER_HPP
#include <iostream>
#include "threadsafequeue.hpp"
#include <thread>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/timestamp.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/avutil.h>
    #include <libavutil/pixdesc.h>
    #include <libavutil/pixfmt.h>
    #include <libavutil/avutil.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/avutil.h>
    #include <libavutil/avutil.h>
    #include "libavcodec/codec.h"
}

static AVPixelFormat hw_pix_fmt_decoder = AV_PIX_FMT_CUDA;

class Decoder
{
private:
    AVCodecContext *pCodecCtx;
    ThreadSafeQueue<AVPacket*> *encoded_queue;
    ThreadSafeQueue<AVFrame*> *decoded_queue;
public:
    Decoder(ThreadSafeQueue<AVPacket*> *encoded_queue, ThreadSafeQueue<AVFrame*> *decoded_queue);
    ~Decoder();
    static AVPixelFormat get_pix_fmt_hw(AVCodecContext* ctx, const AVPixelFormat* fmt);
    void run_send_packet_to_decoder();
    void run_decode();
    std::thread run_send_packet_to_decoder_thread();
    std::thread run_decode_thread();
};


#endif