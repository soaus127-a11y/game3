#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

// DirectX 객체 및 변수
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;

// ---------------------------
// 게임 상태 변수 (수정됨)
// ---------------------------
float g_posX = 0.0f;
bool  g_leftPressed = false;  // A 키 상태
bool  g_rightPressed = false; // D 키 상태
bool  g_isRunning = true;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// HLSL Shader (기존과 동일)
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

// ---------------------------
// Window Message 처리 (수정됨)
// ---------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN: // 키를 눌렀을 때 상태를 true로
        if (wParam == 'A') g_leftPressed = true;
        if (wParam == 'D') g_rightPressed = true;
        if (wParam == VK_ESCAPE) { g_isRunning = false; PostQuitMessage(0); }
        break;

    case WM_KEYUP:   // 키를 뗐을 때 상태를 false로
        if (wParam == 'A') g_leftPressed = false;
        if (wParam == 'D') g_rightPressed = false;
        break;

    case WM_DESTROY:
        g_isRunning = false;
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ------------------------------------------------
// Update: 프레임마다 상태 변수를 보고 위치 이동
// ------------------------------------------------
void Update()
{
    const float moveSpeed = 0.015f; // 프레임당 이동 속도

    // 키가 눌려 있는 상태라면 계속 이동합니다.
    if (g_leftPressed)  g_posX -= moveSpeed;
    if (g_rightPressed) g_posX += moveSpeed;

    // 화면 경계 제한
    if (g_posX < -1.0f) g_posX = -1.0f;
    if (g_posX > 1.0f)  g_posX = 1.0f;
}

// ------------------------------------------------
// Render (기존과 동일)
// ------------------------------------------------
void Render()
{
    float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0, 1 };
    g_pImmediateContext->RSSetViewports(1, &vp);

    Vertex vertices[] = {
        { g_posX + 0.0f,  0.5f, 0.5f, 1, 0, 0, 1 },
        { g_posX + 0.5f, -0.4f, 0.5f, 0, 1, 0, 1 },
        { g_posX - 0.5f, -0.4f, 0.5f, 0, 0, 1, 1 },
        { g_posX + 0.0f, -0.5f, 0.5f, 1, 1, 0, 1 },
        { g_posX - 0.5f,  0.4f, 0.5f, 0, 1, 1, 1 },
        { g_posX + 0.5f,  0.4f, 0.5f, 1, 0, 1, 1 },
    };

    g_pImmediateContext->UpdateSubresource(g_pVertexBuffer, 0, nullptr, vertices, 0, 0);

    UINT stride = sizeof(Vertex), offset = 0;
    g_pImmediateContext->IASetInputLayout(g_pInputLayout);
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->Draw(6, 0);
    g_pSwapChain->Present(1, 0);
}

// ------------------------------------------------
// WinMain (생략: 기존과 동일하게 유지)
// ------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, LoadIcon(NULL, IDI_APPLICATION), LoadCursor(nullptr, IDC_ARROW), (HBRUSH)COLOR_WINDOW, nullptr, L"DX11Window", nullptr };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow(L"DX11Window", L"Smooth Message Move", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1; sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* backBuffer; g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_pRenderTargetView); backBuffer->Release();

    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = { {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}, {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0} };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);

    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 6, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pVertexBuffer);

    MSG msg = {};
    while (g_isRunning) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else { Update(); Render(); }
    }
    return 0;
}