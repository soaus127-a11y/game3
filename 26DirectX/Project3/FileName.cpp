#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <chrono>
#include <vector>
#include <string>

// 라이브러리 링크
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- 전역 DirectX 11 객체 ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// 정점 구조체
struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// --- [A. 게임 엔진 구조: GameObject & Component] ---

class GameObject;

// 추상 클래스 Component
class Component {
public:
    GameObject* pOwner = nullptr;
    virtual void Start() {}
    virtual void Update(float dt) {}
    virtual void Render() {}
    virtual ~Component() {}
};

// GameObject 클래스: 위치 정보 및 컴포넌트 관리
class GameObject {
public:
    std::string name;
    float x = 0.0f, y = 0.0f; // Position 정보
    std::vector<Component*> components;

    GameObject(std::string n, float startX, float startY) : name(n), x(startX), y(startY) {}
    ~GameObject() { for (auto c : components) delete c; }

    void AddComponent(Component* pComp) {
        pComp->pOwner = this;
        components.push_back(pComp);
    }

    // Lecture04 구조: 컴포넌트들의 Update/Render 일괄 호출
    void Update(float dt) { for (auto c : components) c->Update(dt); }
    void Render() { for (auto c : components) c->Render(); }
};

// --- [B. 세부 기능 구현: Renderer & Controller] ---

// 삼각형을 그리는 Renderer 컴포넌트
class TriangleRenderer : public Component {
    ID3D11Buffer* pVB = nullptr;
    ID3D11VertexShader* pVS = nullptr;
    ID3D11PixelShader* pPS = nullptr;
    ID3D11InputLayout* pLayout = nullptr;
    float r, g, b;
    bool isInverted;

public:
    TriangleRenderer(float _r, float _g, float _b, bool inverted)
        : r(_r), g(_g), b(_b), isInverted(inverted) {
    }

    void Start() override {
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

        ID3DBlob* vsBlob, * psBlob;
        D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
        D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

        g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &pVS);
        g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pPS);

        D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        g_pd3dDevice->CreateInputLayout(layoutDesc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pLayout);
        vsBlob->Release(); psBlob->Release();

        D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 3, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        g_pd3dDevice->CreateBuffer(&bd, nullptr, &pVB);
    }

    void Render() override {
        float size = 0.3f;
        float h = size * 0.866f;
        float offset = size * 0.5f;

        Vertex v[3];
        if (!isInverted) { // 정삼각형
            v[0] = { pOwner->x + 0.0f, pOwner->y + size,   0.5f, r, g, b, 1.0f };
            v[1] = { pOwner->x + h,    pOwner->y - offset, 0.5f, r, g, b, 1.0f };
            v[2] = { pOwner->x - h,    pOwner->y - offset, 0.5f, r, g, b, 1.0f };
        }
        else { // 역삼각형
            v[0] = { pOwner->x + 0.0f, pOwner->y - size,   0.5f, r, g, b, 1.0f };
            v[1] = { pOwner->x - h,    pOwner->y + offset, 0.5f, r, g, b, 1.0f };
            v[2] = { pOwner->x + h,    pOwner->y + offset, 0.5f, r, g, b, 1.0f };
        }

        D3D11_MAPPED_SUBRESOURCE ms;
        g_pImmediateContext->Map(pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, v, sizeof(v));
        g_pImmediateContext->Unmap(pVB, 0);

        UINT stride = sizeof(Vertex), off = 0;
        g_pImmediateContext->IASetInputLayout(pLayout);
        g_pImmediateContext->IASetVertexBuffers(0, 1, &pVB, &stride, &off);
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pImmediateContext->VSSetShader(pVS, nullptr, 0);
        g_pImmediateContext->PSSetShader(pPS, nullptr, 0);
        g_pImmediateContext->Draw(3, 0);
    }

    ~TriangleRenderer() {
        if (pVB) pVB->Release(); if (pVS) pVS->Release();
        if (pPS) pPS->Release(); if (pLayout) pLayout->Release();
    }
};

// 프레임 독립적 이동을 담당하는 컴포넌트
class MoveController : public Component {
    int up, down, left, right;
    float speed = 1.5f; // Velocity
public:
    MoveController(int u, int d, int l, int r) : up(u), down(d), left(l), right(r) {}

    void Update(float dt) override {
        // 공식 적용: Position = Position + (Velocity * DeltaTime)
        if (GetAsyncKeyState(up) & 0x8000) pOwner->y += speed * dt;
        if (GetAsyncKeyState(down) & 0x8000) pOwner->y -= speed * dt;
        if (GetAsyncKeyState(left) & 0x8000) pOwner->x -= speed * dt;
        if (GetAsyncKeyState(right) & 0x8000) pOwner->x += speed * dt;
    }
};

// --- [C. 시스템 제어: 윈도우 프로시저] ---

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0); // ESC 종료
        if (wParam == 'F') { // F 키: 전체화면 토글
            static bool isFull = false;
            isFull = !isFull;
            g_pSwapChain->SetFullscreenState(isFull, nullptr);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// --- [D. 메인 엔트리: Game Loop] ---

int main() {
    // 1. Win32 초기화 (800x600 Fixed)
    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"DX11_Assign", nullptr };
    RegisterClassEx(&wc);

    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindow(L"DX11_Assign", L"P1: Arrows | P2: WASD | F: Fullscreen | ESC: Exit",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
    ShowWindow(hWnd, SW_SHOW);

    // 2. DX11 디바이스 및 스왑체인 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBack;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBack);
    g_pd3dDevice->CreateRenderTargetView(pBack, nullptr, &g_pRenderTargetView);
    pBack->Release();

    // 3. GameWorld 구성
    std::vector<GameObject*> gameWorld;

    // Player 1 (상향 삼각형, 빨간색, 방향키)
    GameObject* p1 = new GameObject("Player1", 0.0f, 0.0f);
    p1->AddComponent(new TriangleRenderer(1.0f, 0.2f, 0.2f, false));
    p1->AddComponent(new MoveController(VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT));
    gameWorld.push_back(p1);

    // Player 2 (하향 삼각형, 노란색, WASD)
    GameObject* p2 = new GameObject("Player2", 0.0f, 0.0f);
    p2->AddComponent(new TriangleRenderer(1.0f, 1.0f, 0.0f, true));
    p2->AddComponent(new MoveController('W', 'S', 'A', 'D'));
    gameWorld.push_back(p2);

    for (auto obj : gameWorld) for (auto comp : obj->components) comp->Start();

    // 4. 고해상도 타이머 기반 Non-blocking Game Loop
    auto prevTime = std::chrono::high_resolution_clock::now();
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [DeltaTime 계산]
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - prevTime).count();
            prevTime = now;

            // 렌더링 준비
            float clearColor[] = { 0.1f, 0.1f, 0.15f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0, 1 };
            g_pImmediateContext->RSSetViewports(1, &vp);

            // [GameWorld Update & Render]
            for (auto obj : gameWorld) {
                obj->Update(dt); // 위치 갱신 (Velocity * dt 적용됨)
                obj->Render();   // 그리기 호출
            }

            g_pSwapChain->Present(1, 0);
        }
    }

    // 5. 정리 (메모리 해제 및 리소스 Release)
    for (auto obj : gameWorld) delete obj;
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) {
        g_pSwapChain->SetFullscreenState(FALSE, nullptr); // 전체화면 해제 후 릴리즈 권장
        g_pSwapChain->Release();
    }
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return 0;
}