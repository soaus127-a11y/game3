#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include "Logger.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

class GraphicsContext {
public:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    bool Init(HWND hWnd, int w, int h) {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;

        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &swapChain, &device, nullptr, &context);

        ID3D11Texture2D* pBackBuffer;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        device->CreateRenderTargetView(pBackBuffer, nullptr, &rtv);
        pBackBuffer->Release();

        context->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp = { 0, 0, (float)w, (float)h, 0.0f, 1.0f };
        context->RSSetViewports(1, &vp);
        return true;
    }

    void BeginScene() {
        float color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        context->ClearRenderTargetView(rtv, color);
    }
    void EndScene() { swapChain->Present(1, 0); }
};