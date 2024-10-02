#include <d3d11.h>
#include <dxgi1_2.h>
#include <iostream>
#include "renderer.hpp"
#include "threadsafequeue.hpp"
#include "encoder.hpp"
#include "decoder.hpp"
// #include "decoder.hpp"
void on_queue_full(uint8_t* frame) {
    std::cout << "Queue is full" << std::endl;
    free(frame);
}
static ThreadSafeQueue<uint8_t*> queue(10, on_queue_full);

void on_decoder_queue_full(AVPacket* packet) {
    av_packet_free(&packet);
}

void on_render_queue_full(AVFrame* frame) {
    av_frame_free(&frame);
}
static ThreadSafeQueue<AVPacket*> encoded_buffer(20, on_decoder_queue_full);
static ThreadSafeQueue<AVFrame*> render_buffer(20, on_render_queue_full);
static int record_screen_buffer(IDXGIOutputDuplication* deskDuplication, ID3D11Device *device, ID3D11DeviceContext *context) {
    std::cout << "Starting screen capture" << std::endl;

    while (true) {  // Changed to true for continuous capture
        IDXGIResource* desktopResource = nullptr;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        HRESULT hr = deskDuplication->AcquireNextFrame(500, &frameInfo, &desktopResource);
        // HRESULT hr;
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            continue;  // No new frame, try again
        } else if (FAILED(hr)) {
            std::cerr << "Failed to acquire next frame" << std::endl;
            return -1;
        }

        ID3D11Texture2D* desktopTexture = nullptr;
        hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&desktopTexture);
        if (FAILED(hr)) {
            std::cerr << "Failed to query interface for ID3D11Texture2D" << std::endl;
            desktopResource->Release();
            deskDuplication->ReleaseFrame();
            return -1;
        }

        D3D11_TEXTURE2D_DESC desc;
        desktopTexture->GetDesc(&desc);
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;

        ID3D11Texture2D* stagingTexture = nullptr;
        hr = device->CreateTexture2D(&desc, nullptr, &stagingTexture);
        if (FAILED(hr)) {
            std::cerr << "Failed to create staging texture" << std::endl;
            desktopTexture->Release();
            desktopResource->Release();
            deskDuplication->ReleaseFrame();
            return -1;
        }

        context->CopyResource(stagingTexture, desktopTexture);

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        hr = context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
        if (SUCCEEDED(hr)) {
            size_t bufferSize = desc.Height * desc.Width * 4;  // Assuming RGBA
            uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * bufferSize);
            memcpy(buffer, mappedResource.pData, bufferSize);
            queue.push(buffer);

            context->Unmap(stagingTexture, 0);
        } else {
            std::cerr << "Failed to map the staging texture" << std::endl;
        }

        stagingTexture->Release();
        desktopTexture->Release();
        desktopResource->Release();
        deskDuplication->ReleaseFrame();
    }

    return 0;
}


int record_screen_buffer_() {

}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, 
                                 D3D11_SDK_VERSION, &device, &featureLevel, &context))) {
        std::cerr << "Failed to create D3D11 device" << std::endl;
        return -1;
    }

    // Create DXGI Factory
    IDXGIDevice* dxgiDevice = nullptr;
    device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);

    IDXGIAdapter* adapter = nullptr;
    dxgiDevice->GetAdapter(&adapter);

    IDXGIOutput* output = nullptr;
    adapter->EnumOutputs(0, &output);

    IDXGIOutput1* output1 = nullptr;
    output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);

    // Duplicate the desktop
    IDXGIOutputDuplication* deskDuplication = nullptr;
    if (FAILED(output1->DuplicateOutput(device, &deskDuplication))) {
        std::cerr << "Failed to duplicate output" << std::endl;
        return -1;
    }
    std::thread capture(record_screen_buffer, deskDuplication, device, context);

    // static Renderer renderer(&queue);
    // renderer.run_thread().join();
    Encoder encoder(&queue, &encoded_buffer); 
    Decoder decoder(&encoded_buffer, &render_buffer);
    Renderer renderer(&render_buffer);
    auto encoder_thread = encoder.start_encoder_thread();
    // auto send_encoded_thread = encoder.start_send_encoded_to_decode();
    
    // auto send_packet_to_decoder_thread = decoder.run_send_packet_to_decoder_thread();
    auto decode_thread = decoder.run_decode_thread();
    auto render_thread = renderer.run_thread();
    capture.join();
    encoder_thread.join();
    decode_thread.join();
    render_thread.join();
    // send_encoded_thread.join();
    // send_packet_to_decoder_thread.join();
    // decode_thread.join();
    // render.join();
    return 0;
}