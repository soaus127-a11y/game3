#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <chrono>
#include <string>
#include <thread>

// 링커 에러 방지 (LNK1120 해결용)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:windows")

// 전역 객체
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;

// 게임 상태 변수
float g_posX = 0.0f;
bool  g_leftPressed = false;
bool  g_rightPressed = false;
bool  g_isRunning = true;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// HLSL Shader
const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1);
    output.col = input.col;
    return output;
}
float4 PS(PS_INPUT input) : SV_Target { return input.col; }
)";

// 메시지 처리
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        if (wParam == 'A') g_leftPressed = true;
        if (wParam == 'D') g_rightPressed = true;
        if (wParam == VK_ESCAPE) { g_isRunning = false; PostQuitMessage(0); }
        break;
    case WM_KEYUP:
        if (wParam == 'A') g_leftPressed = false;
        if (wParam == 'D') g_rightPressed = false;
        break;
    case WM_DESTROY: g_isRunning = false; PostQuitMessage(0); break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// WinMain 진입점
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 1. 윈도우 생성
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance,
                     nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"DX11Star", nullptr };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow(L"DX11Star", L"DirectX11 - 45 FPS Fixed", WS_OVERLAPPEDWINDOW,
        100, 100, 800, 600, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    // 2. D3D11 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1; sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // 3. 셰이더 및 버퍼 세팅
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);

    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 6, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pVertexBuffer);

    // 4. 타이머 변수 (45 FPS 목표)
    const float targetFPS = 45.0f;
    const float targetFrameTime = 1.0f / targetFPS; // 약 0.02222초
    auto prevTime = std::chrono::steady_clock::now();

    MSG msg = {};
    while (g_isRunning) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [A] 프레임 시작 시간
            auto frameStart = std::chrono::steady_clock::now();
            std::chrono::duration<float> elapsed = frameStart - prevTime;
            float dt = elapsed.count();
            prevTime = frameStart;

            // [B] Update (DeltaTime 적용)
            const float moveSpeed = 1.5f; // 초당 1.5 유닛 이동
            if (g_leftPressed)  g_posX -= moveSpeed * dt;
            if (g_rightPressed) g_posX += moveSpeed * dt;
            if (g_posX < -1.2f) g_posX = 1.2f; // 화면 밖으로 나가면 반대편에서 등장 (순환 예시)
            if (g_posX > 1.2f)  g_posX = -1.2f;

            // [C] Render
            float clearColor[] = { 0.1f, 0.15f, 0.2f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0, 1 };
            g_pImmediateContext->RSSetViewports(1, &vp);

            Vertex vertices[] = {
                { g_posX + 0.0f,  0.5f, 0.5f, 1, 0, 0, 1 }, { g_posX + 0.5f, -0.4f, 0.5f, 0, 1, 0, 1 }, { g_posX - 0.5f, -0.4f, 0.5f, 0, 0, 1, 1 },
                { g_posX + 0.0f, -0.5f, 0.5f, 1, 1, 0, 1 }, { g_posX - 0.5f,  0.4f, 0.5f, 0, 1, 1, 1 }, { g_posX + 0.5f,  0.4f, 0.5f, 1, 0, 1, 1 },
            };
            g_pImmediateContext->UpdateSubresource(g_pVertexBuffer, 0, nullptr, vertices, 0, 0);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
            g_pImmediateContext->Draw(6, 0);

            g_pSwapChain->Present(0, 0); // VSync Off (수동 제어)

            // [D] FPS 표시 및 고정 로직
            float currentFPS = (dt > 0) ? (1.0f / dt) : 0;
            std::wstring title = L"FPS: " + std::to_wstring((int)currentFPS) + L" | Target: 45";
            SetWindowText(hWnd, title.c_str());

            // 🔥 정밀 대기 (45 FPS 고정 핵심)
            while (true) {
                auto now = std::chrono::steady_clock::now();
                float workDone = std::chrono::duration<float>(now - frameStart).count();
                if (workDone >= targetFrameTime) break;

                // 시간이 많이 남았으면 Sleep으로 CPU 쉬게 해줌
                if (targetFrameTime - workDone > 0.002f) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
    }
    return 0;
}