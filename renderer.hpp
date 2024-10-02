#ifndef RENDERER_HPP
#define RENDERER_HPP
#include "threadsafequeue.hpp"
#include <iostream>
#include <thread>
#include <chrono>
extern "C" {
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_video.h>
    #include <libavcodec/avcodec.h>
}

class Renderer
{
private:
    ThreadSafeQueue<AVFrame*>* frame_buffer;
    SDL_Window* windows;
    SDL_Texture* texture;
    SDL_Renderer* renderer;
    const int WIDTH = 1000;
    const int HEIGH = 600;
    const int DESKTOP_WIDTH = 2560;
    const int DESKTOP_HEIGH = 1440;
    const int POS_X = 100;
    const int POS_Y = 100;
public:
    Renderer(ThreadSafeQueue<AVFrame*> *buffer);
    ~Renderer();
    void run();
    std::thread run_thread();

};

#endif // RENDERER_HPP


