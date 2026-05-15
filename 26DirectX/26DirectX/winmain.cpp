#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <chrono>
#include <stdio.h> // printf 사용
#include <string>

// 링커 에러 방지 및 콘솔창 활성화 (요구사항: printf 사용을 위해 subsystem:console 설정)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(linker, "/subsystem:console")

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
float g_posY = 0.0f; // 상하 이동 추가
bool g_keys[256] = { false }; // 키 상태 배열로 관리
bool g_isRunning = true;

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
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}
float4 PS(PS_INPUT input) : SV_Target { return input.col; }
)";

// 메시지 처리
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        if (wParam < 256) g_keys[wParam] = true;
        if (wParam == VK_ESCAPE) { g_isRunning = false; PostQuitMessage(0); }
        break;
    case WM_KEYUP:
        if (wParam < 256) g_keys[wParam] = false;
        break;
    case WM_DESTROY: g_isRunning = false; PostQuitMessage(0); break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 진입점 (subsystem:console이므로 main 사용, 내부에서 WinMain 로직 수행 가능)
int main() {
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // 1. 윈도우 생성 (600x600 픽셀 손실 방지)
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance,
                      nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"DX11Hexagram", nullptr };
    RegisterClassEx(&wc);

    // 실제 렌더링 영역이 600x600이 되도록 윈도우 크기 계산
    RECT rc = { 0, 0, 600, 600 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindow(L"DX11Hexagram", L"DirectX11 - 600x600 Hexagram", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, SW_SHOW);

    // 2. D3D11 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 600;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

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

    // 육망성은 삼각형 2개(정점 6개)로 구성
    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 6, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pVertexBuffer);

    // 4. GameLoop
    auto prevTime = std::chrono::high_resolution_clock::now();
    float fpsTimer = 0.0f;
    int frameCount = 0;

    MSG msg = {};
    while (g_isRunning) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [A] DeltaTime 측정 시작
            auto frameStart = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = frameStart - prevTime;
            float dt = elapsed.count();
            prevTime = frameStart;

            // 요구사항: printf를 이용해서 deltatime 출력
            printf("DeltaTime: %.6f\n", dt);

            // 요구사항: 1초마다 FPS 출력
            fpsTimer += dt;
            frameCount++;
            if (fpsTimer >= 1.0f) {
                printf(">> FPS: %d\n", frameCount);
                frameCount = 0;
                fpsTimer -= 1.0f;
            }

            // [B] Update (WASD 상하좌우 이동)
            const float moveSpeed = 1.0f;
            if (g_keys['W']) g_posY += moveSpeed * dt;
            if (g_keys['S']) g_posY -= moveSpeed * dt;
            if (g_keys['A']) g_posX -= moveSpeed * dt;
            if (g_keys['D']) g_posX += moveSpeed * dt;

            // [C] Render
            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // 육망성 정점 데이터 (정삼각형 2개 조합)
            float size = 0.3f;
            Vertex vertices[] = {
                // 삼각형 1 (상향)
                { g_posX + 0.0f, g_posY + size, 0.5f, 1, 0, 0, 1 },
                { g_posX + size * 0.86f, g_posY - size * 0.5f, 0.5f, 0, 1, 0, 1 },
                { g_posX - size * 0.86f, g_posY - size * 0.5f, 0.5f, 0, 0, 1, 1 },
                // 삼각형 2 (하향)
                { g_posX + 0.0f, g_posY - size, 0.5f, 1, 1, 0, 1 },
                { g_posX - size * 0.86f, g_posY + size * 0.5f, 0.5f, 0, 1, 1, 1 },
                { g_posX + size * 0.86f, g_posY + size * 0.5f, 0.5f, 1, 0, 1, 1 },
            };

            // 동적 버퍼 업데이트 (D3D11_USAGE_DYNAMIC)
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            g_pImmediateContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            memcpy(mappedResource.pData, vertices, sizeof(vertices));
            g_pImmediateContext->Unmap(g_pVertexBuffer, 0);

            D3D11_VIEWPORT vp = { 0, 0, 600, 600, 0, 1 };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

            g_pImmediateContext->Draw(6, 0);

            g_pSwapChain->Present(0, 0); // VSync Off로 설정하여 최대 성능 확인 가능
        }
    }

    // 정리
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return 0;
}