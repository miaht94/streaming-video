#include <iostream>
#include "encoder.hpp"

static int decoder_width = 2560;
static int decoder_height = 1440;
Encoder::Encoder(ThreadSafeQueue<uint8_t*>* frame_queue, ThreadSafeQueue<AVPacket*> *encode_queue) {
    this->frame_queue = frame_queue;
    this->encode_queue = encode_queue;
    const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (codec == nullptr) {
        std::cerr << "Codec not found" << std::endl;
    }
    
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    
    hw_pix_fmt_encoder = AV_PIX_FMT_CUDA;
    const AVHWDeviceType hw_dev_type = AV_HWDEVICE_TYPE_CUDA;
    AVBufferRef* hw_dev_ctx = nullptr;
    AVDictionary* opts = nullptr;
    av_hwdevice_ctx_create(&hw_dev_ctx, hw_dev_type, nullptr, opts, 0);
    codec_ctx->hw_device_ctx = av_buffer_ref(hw_dev_ctx);
    scaler_ctx = sws_getContext(2560, 1440, AV_PIX_FMT_BGRA, 2560, 1440, AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr, nullptr, nullptr);
    codec_ctx->time_base = AVRational{1, 60};
    codec_ctx->framerate = AVRational{60, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_NV12;
    codec_ctx->width = 2560;
    codec_ctx->height = 1440;
    codec_ctx->sample_aspect_ratio = AVRational{1, 1};
    codec_ctx->framerate = AVRational{60, 1};
    codec_ctx->get_format = get_pix_fmt_hw;
    int ret = avcodec_open2(codec_ctx, codec, nullptr);
    
    this->codec_ctx = codec_ctx;
    std::cout << ret << std::endl;
    ret;
}


AVPixelFormat Encoder::get_pix_fmt_hw(AVCodecContext* ctx, const AVPixelFormat* fmt)
{
    const enum AVPixelFormat *p;
    for (p = fmt; *p != -1; p++) {
        if (*p == hw_pix_fmt_encoder) {
            return *p;
        }
    }

    std::cerr << "Failed to get HW surface format.\n";
    return AV_PIX_FMT_NONE;
}

void Encoder::encode() {
    long long frame_count = 0;
    while (true) {
        uint8_t* img = nullptr;
        bool pop_result = this->frame_queue->try_pop(img);
        if (!pop_result) {
            continue;
        }
        AVFrame* frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_BGRA;
        frame->width = 2560;
        frame->height = 1440;
        AVFrame* dst_frame = av_frame_alloc();

        if (av_frame_get_buffer(frame, 0) != 0) {
            std::cerr << "Failed to allocate frame buffer" << std::endl;
            break;
        }
        int fill_ret = av_image_fill_arrays(frame->data, frame->linesize, (const uint8_t*)img, AV_PIX_FMT_ARGB, 2560, 1440, 1);
        frame->pts = frame_count;
        int scale_ret = sws_scale_frame(this->scaler_ctx, dst_frame, frame);
        if (scale_ret < 0) {
            std::cerr << "Failed to scale frame" << std::endl;
            break;
        }
        dst_frame->pts = frame_count;
        dst_frame->pict_type = AV_PICTURE_TYPE_NONE;
        int ret = avcodec_send_frame(codec_ctx, dst_frame);
        frame_count+=1000;
        
        AVPacket* packet = av_packet_alloc();
        int ret_ = avcodec_receive_packet(codec_ctx, packet);
        // frame_count = frame_count;
        av_frame_free(&frame);
        av_frame_free(&dst_frame);
        free(img);
        std::cout << ret_ << std::endl;
        switch(ret_) {
            case 0:
                //send packet to decoder
                this->encode_queue->push(packet);
                break;
            case AVERROR(EAGAIN):
                av_packet_free(&packet);
                break;
            case AVERROR_EOF:
                av_packet_free(&packet);
                break;
            case AVERROR(EINVAL):
                av_packet_free(&packet);
                std::cerr << "Invalid encoder" << std::endl;
                break;
            default:
                // av_packet_free(&packet);
                break;
        }
    }
}



std::thread Encoder::start_encoder_thread() {
    std::thread encoder_thread(&Encoder::encode, this);
    return encoder_thread;
}

void Encoder::send_encode_to_decode() {
    while (true)
    {
        AVPacket* packet = av_packet_alloc();
        
        int ret = avcodec_receive_packet(codec_ctx, packet);
        switch(ret) {
            case 0:
                //send packet to decoder
                this->encode_queue->push(packet);
                break;
            case AVERROR(EAGAIN):
                // av_packet_free(&packet);
                continue;
            case AVERROR_EOF:
                // av_packet_free(&packet);
                break;
            case AVERROR(EINVAL):
                // av_packet_free(&packet);
                std::cerr << "Invalid encoder" << std::endl;
                break;
            default:
                // av_packet_free(&packet);
                break;
        }
            
    }
    
}

std::thread Encoder::start_send_encoded_to_decode() {
    std::thread send_thread(&Encoder::send_encode_to_decode, this);
    return send_thread;
}

Encoder::~Encoder() {
    avcodec_free_context(&codec_ctx);
    sws_freeContext(scaler_ctx);
}