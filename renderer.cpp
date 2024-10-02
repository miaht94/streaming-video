#include "renderer.hpp"
#include "threadsafequeue.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
extern "C" {
    #include <SDL2/SDL.h>
}
Renderer::Renderer(ThreadSafeQueue<AVFrame*> *queue_buffer) {
    SDL_Init(SDL_INIT_VIDEO);
    this->frame_buffer = queue_buffer;
    this->windows = SDL_CreateWindow("SDL2 Renderer", this->POS_X, this->POS_Y, this->WIDTH, this->HEIGH, SDL_WINDOW_SHOWN);
    if (!this->windows) {
        std::cerr << "Could not create windows for render SDL2: " << SDL_GetError() << std::endl;
    }
    this->renderer = SDL_CreateRenderer(this->windows, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        std::cerr << "Could not create renderer: " << SDL_GetError() << std::endl;
    }
    this->texture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING, this->WIDTH, this->HEIGH);
    if (!this->texture) {
        std::cerr << "Can not init texture" << SDL_GetError() << std::endl;
    }
    return;
}

Renderer::~Renderer() {
    SDL_DestroyWindowSurface(this->windows);
    SDL_DestroyWindow(this->windows);
    SDL_DestroyRenderer(this->renderer);
    SDL_DestroyTexture(this->texture);
}

void Renderer::run() {
    std::cout << "Renderer thread started" << std::endl;
    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
        }
        AVFrame* frame_ = nullptr;
        bool pop_result = this->frame_buffer->try_pop(frame_);
        if (!pop_result) {
            continue;
        }
        SDL_Rect rect = {0, 0, 2560, 1440};
        SDL_SetRenderDrawColor(this->renderer, 255, 255, 255, 255);
        SDL_RenderClear(this->renderer);
        SDL_UpdateNVTexture(this->texture, &rect, frame_->data[0], frame_->linesize[0], frame_->data[1], frame_->linesize[1]);
        SDL_RenderCopy(renderer, texture, &rect, &rect);
        SDL_RenderPresent(this->renderer);
        av_frame_free(&frame_);
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::thread Renderer::run_thread() {
    std::thread render_thread(&Renderer::run, this);
    return render_thread;
}
