#include <windows.h>
#include <iostream>
#include <thread>
#include <mutex>
#include "threadsafequeue.hpp"
#include <chrono>
extern "C" {
    #include <SDL2/SDL.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libswscale/swscale.h> 
    #include <libavutil/hwcontext.h>
    #include <libavutil/error.h>
}
auto start = std::chrono::high_resolution_clock::now();
static ThreadSafeQueue<AVFrame*> queue;
static enum AVPixelFormat hw_pix_fmt;
static AVBufferRef *hw_device_ctx = nullptr;
static AVFrame *render_frame = nullptr;
static std::mutex *render_frame_mutex = new std::mutex();

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type) {
    int err = av_hwdevice_ctx_create(&hw_device_ctx, type, nullptr, nullptr, 0);
    if (err < 0) {
        std::cerr << "Failed to create hardware device: " << std::endl;
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return 0;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt) {
            return *p;
        }
    }

    std::cerr << "Failed to get HW surface format.\n";
    return AV_PIX_FMT_NONE;
}

void send_packet_to_decoder(AVFormatContext *pFormatContext, AVCodecContext *decoder_ctx, AVPacket *packet, std::mutex *decoder_ctx_lock) {
    while (av_read_frame(pFormatContext, packet) >= 0) {
        std::cout << "Decoded a new frame";
        std::lock_guard<std::mutex> lock(*decoder_ctx_lock);
        avcodec_send_packet(decoder_ctx, packet);
        
        // av_packet_unref(packet);
    }

    // Send EOF packet to decoder after all
    packet->data = NULL;
    packet->size = 0;
    avcodec_send_packet(decoder_ctx, packet);
    // return 0;
}

void decode_receive_frame(AVCodecContext *codec_context, AVFrame *frame, AVFrame *sw_frame, std::mutex *decoder_ctx_lock) {
    while (true) {
        std::lock_guard<std::mutex> lock(*decoder_ctx_lock);
        auto out = avcodec_receive_frame(codec_context, frame);
        if (out == AVERROR(EAGAIN)) {
            // std::cout << "Not ready yet";
            continue;
        }
        if (out == AVERROR(EINVAL)) {
            std::cerr << "Codec have not opened yet";
            break;
        }
        if (out == AVERROR_EOF) {
            std::cout << "End of file";
             auto end = std::chrono::high_resolution_clock::now();

            // Calculate the duration in milliseconds
            std::chrono::duration<double, std::milli> elapsed = end - start;
            std::cout << "Time elapsed " << elapsed.count() << std::endl;
            break;
        }
        if (frame->format == hw_pix_fmt) {
            // Use the hardware-accelerated frame (e.g., CUDA frame)
            std::cout << "Hardware accelerated frame decoded.\n";
            if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) {
                std::cerr << "Error transferring data to CPU.\n";
                break;
            }
            AVFrame *temp = av_frame_alloc();
            av_frame_ref(temp, sw_frame);
            av_frame_unref(sw_frame);
            queue.push(temp);
            std::cout << "Software frame decoded.\n";
        }
    }
}

void render_frame_using_sdl2() {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Init(SDL_INIT_VIDEO);
    
    window = SDL_CreateWindow("SDL2 Renderer", 100, 100, 2560, 1440, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        std::cerr << "Could not create window: " << SDL_GetError() << std::endl;
        return;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
     if (renderer == NULL) {
        std::cerr << "Could not create renderer: " << SDL_GetError() << std::endl;
        return;
    }
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING, 2560, 1440);
    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            // User requests to quit
            if (event.type == SDL_QUIT) {
                quit = true;
            }
        }
        AVFrame *frame = NULL;
        bool pop_result = queue.try_pop(frame);
        if (!pop_result) {
            std::cout << "Render queue is empty" << std::endl;
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        SDL_Rect rect = {0, 0, 2560, 1440};
        SDL_UpdateNVTexture(texture, &rect, frame->data[0], 2560, frame->data[1], 2560);
        SDL_RenderCopy(renderer, texture, &rect, &rect);
        SDL_RenderPresent(renderer);
        av_frame_free(&frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindowSurface(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    
    AVFormatContext *pFormatContext = avformat_alloc_context();
    if (avformat_open_input(&pFormatContext, "video.mp4", NULL, NULL) < 0) {
        std::cerr << "Can not open media file";
    };
    
    if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
        std::cerr << "Can not find stream info for media file";
    }
    const char* decoder_name = "h264";
    const AVCodec *decoder = avcodec_find_decoder_by_name(decoder_name);
    if (!decoder) {
        std::cerr << "Can not find decoder \"" << decoder_name << "\"";
    }

    auto codec_context = avcodec_alloc_context3(decoder);
    if (!codec_context) {
        std::cerr << "Can not allocate avcodec context";
    }
    auto video_stream = pFormatContext->streams[0];
    avcodec_parameters_to_context(codec_context, video_stream->codecpar);
    // Initialize the hardware decoder
    hw_pix_fmt = AV_PIX_FMT_CUDA;
    codec_context->get_format = get_hw_format;

    if (hw_decoder_init(codec_context, AV_HWDEVICE_TYPE_CUDA) < 0) {
        std::cerr << "Failed to initialize hardware decoder.\n";
        return -1;
    }
    avcodec_open2(codec_context, decoder, NULL);
    AVFrame *frame = av_frame_alloc();
    AVFrame *sw_frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    std::mutex decoder_ctx_lock;
    std::thread send_decoder_thread(send_packet_to_decoder, pFormatContext, codec_context, packet, &decoder_ctx_lock);
    std::thread receive_frame(decode_receive_frame, codec_context, frame, sw_frame, &decoder_ctx_lock);
    std::thread render_thread(render_frame_using_sdl2);
    send_decoder_thread.join();
    receive_frame.join();
    render_thread.join();
    std::cout << "[DONE]";
    // Defer
    av_frame_free(&frame);
    av_frame_free(&render_frame);
    av_packet_free(&packet);
    av_buffer_unref(&hw_device_ctx);
    avcodec_close(codec_context);
    avformat_close_input(&pFormatContext);
    return 0;
}