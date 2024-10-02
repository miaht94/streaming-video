#include "decoder.hpp"


AVPixelFormat Decoder::get_pix_fmt_hw(AVCodecContext* ctx, const AVPixelFormat* fmt)
{
    const enum AVPixelFormat *p;
    for (p = fmt; *p != -1; p++) {
        if (*p == hw_pix_fmt_decoder) {
            return *p;
        }
    }

    std::cerr << "Failed to get HW surface format.\n";
    return AV_PIX_FMT_NONE;
}

Decoder::Decoder(ThreadSafeQueue<AVPacket*>* encoded_queue, ThreadSafeQueue<AVFrame*> *decoded_queue): encoded_queue(encoded_queue), decoded_queue(decoded_queue) {
    const AVCodec* pAVCodec = avcodec_find_decoder_by_name("h264_cuvid");
    AVCodecContext* pAVCodecContext = avcodec_alloc_context3(pAVCodec);
    AVBufferRef* pHWDeviceContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    AVBufferRef *hw_device_ctx = nullptr;
    av_hwdevice_ctx_create(&pHWDeviceContext, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    pAVCodecContext->hw_device_ctx = av_buffer_ref(pHWDeviceContext);
    pAVCodecContext->get_format = get_pix_fmt_hw;
    pAVCodecContext->width = 2560;
    pAVCodecContext->height = 1440;
    pAVCodecContext->time_base = AVRational{1, 60};
    pAVCodecContext->framerate = AVRational{60, 1};
    pAVCodecContext->sample_aspect_ratio = AVRational{1, 1};
    pAVCodecContext->framerate = AVRational{60, 1};
    pAVCodecContext->pix_fmt = AV_PIX_FMT_NV12;
    int open_ret = avcodec_open2(pAVCodecContext, pAVCodec, NULL);
    this->pCodecCtx = pAVCodecContext;
}

void Decoder::run_send_packet_to_decoder() {
    while (true) {
        
    }
}

void Decoder::run_decode() {
    
    while (true) {
        AVPacket* packet = nullptr;
        AVFrame* frame = av_frame_alloc();
        AVFrame* sw_frame = av_frame_alloc();
        bool pop_result = encoded_queue->try_pop(packet);
        if (!pop_result) {
            continue;
        }
        
        int ret_send = avcodec_send_packet(pCodecCtx, packet);
        int ret = avcodec_receive_frame(pCodecCtx, frame);
        if (ret_send < 0) {
            std::cerr << "Error sending packet for decoding" << std::endl;
            goto free_resource;
        }
        if (ret == AVERROR(EAGAIN)) {
            std::cout << "Frame is not available yet." << std::endl;
            goto free_resource;
        } else if (ret == AVERROR_EOF ) {
            std::cout << "End of stream reached." << std::endl;
            goto free_resource;
        } else if (ret < 0) {
            std::cerr << "Error decoding frame" << std::endl;
            goto free_resource;
        }
        if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                std::cerr << "Error transferring the data to system memory" << std::endl;
                av_frame_free(&sw_frame);
                goto free_resource;
        }
        std::cout << "Frame decoded successfully." << std::endl;
        free_resource:
        av_packet_free(&packet);
        av_frame_free(&frame);
        this->decoded_queue->push(sw_frame);
        // av_frame_free(&sw_frame);
    }
}

std::thread Decoder::run_decode_thread() {
    std::thread decode_thread(&Decoder::run_decode, this);
    return decode_thread;
}

std::thread Decoder::run_send_packet_to_decoder_thread() {
    std::thread send_packet_thread(&Decoder::run_send_packet_to_decoder, this);
    return send_packet_thread;
}

Decoder::~Decoder() {
    avcodec_free_context(&pCodecCtx);
}

