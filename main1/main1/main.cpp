#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include "Logger.hpp"
#include "InfoButton.hpp"

// 라이브러리 링크: DirectX 11 및 쉐이더 컴파일러
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- 전역 변수 설정 (DirectX 11 핵심 객체) ---
ID3D11Device* g_device = nullptr;     // 리소스(버퍼, 쉐이더) 생성
ID3D11DeviceContext* g_context = nullptr;    // 렌더링 명령 실행
IDXGISwapChain* g_swapChain = nullptr;  // 더블 버퍼링 관리
ID3D11RenderTargetView* g_rtv = nullptr;        // 최종 출력 도화지
ID3D11VertexShader* g_vs = nullptr;         // 정점 쉐이더
ID3D11PixelShader* g_ps = nullptr;         // 픽셀 쉐이더
ID3D11InputLayout* g_layout = nullptr;     // 정점 데이터 구조
ID3D11Buffer* g_vbo = nullptr;        // 정점 버퍼 (사각형 좌표 저장)

// 정점 데이터 구조체
struct Vertex { DirectX::XMFLOAT3 pos; };

// 윈도우 메시지 핸들러
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    // [1] 디버깅용 콘솔 할당 및 로그 시스템 초기화
    AllocConsole();
    freopen("CONOUT$", "w", stdout);

    if (!Logger::Get()->Initialize("engine_log.txt")) return 0;
    Logger::Get()->Log("Engine Initialization Started.");
    Logger::Get()->Log("[Engine] GameLoop Created.");

    // [2] 윈도우 클래스 등록 및 창 생성
    const wchar_t* className = L"DX11_FSM_WINDOW";
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = className;
    RegisterClass(&wc);

    HWND hWnd = CreateWindow(className, L"DX11 FSM Button Project",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) return 0;
    ShowWindow(hWnd, nShow);

    // [3] DirectX 11 장치 및 스왑 체인 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, nullptr, &g_context);

    // 렌더 타겟 뷰 생성 (Back Buffer 바인딩)
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_device->CreateRenderTargetView(pBackBuffer, nullptr, &g_rtv);
    pBackBuffer->Release();

    // [4] 쉐이더 컴파일 및 입력 레이아웃 설정
    ID3DBlob* vsBlob = nullptr;
    // 쉐이더 파일(effect.hlsl)은 반드시 exe와 같은 폴더에 있어야 함
    HRESULT hr = D3DCompileFromFile(L"effect.hlsl", nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, nullptr);
    if (FAILED(hr)) {
        MessageBox(nullptr, L"effect.hlsl 파일을 찾을 수 없습니다!", L"Shader Error", MB_OK);
        return 0;
    }
    g_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vs);

    D3D11_INPUT_ELEMENT_DESC ied[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    g_device->CreateInputLayout(ied, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_layout);
    vsBlob->Release();

    ID3DBlob* psBlob = nullptr;
    D3DCompileFromFile(L"effect.hlsl", nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, nullptr);
    g_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_ps);
    psBlob->Release();

    // [5] 정사각형 정점 데이터 생성 (NDC 좌표 기준 중앙에 위치)
    Vertex vertices[] = {
        { {-0.4f,  0.4f, 0.0f} }, { { 0.4f,  0.4f, 0.0f} }, { {-0.4f, -0.4f, 0.0f} },
        { {-0.4f, -0.4f, 0.0f} }, { { 0.4f,  0.4f, 0.0f} }, { { 0.4f, -0.4f, 0.0f} }
    };
    D3D11_BUFFER_DESC vbd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA vsd = { vertices };
    g_device->CreateBuffer(&vbd, &vsd, &g_vbo);

    // [6] 버튼 로직 객체 생성 (윈도우 800x600 픽셀 기준 중앙 충돌 영역 설정)
    // 윈도우 좌표 기준: x=240~560, y=180~420 영역이 중앙 사각형에 해당
    InfoButton btn(g_device, 240.0f, 180.0f, 320.0f, 240.0f);

    // 뷰포트 설정
    D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
    g_context->RSSetViewports(1, &vp);

    Logger::Get()->Log("Game Loop is starting...");

    // [7] 메인 메시지 루프
    MSG msg = { 0 };
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 마우스 좌표 가져오기
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(hWnd, &p);
            bool isLDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000);

            // 1. 로직 업데이트 (FSM 상태 체크 및 색상 결정)
            btn.Update(p.x, p.y, isLDown);

            // 2. 화면 초기화 (배경색: 어두운 회색)
            float clearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
            g_context->ClearRenderTargetView(g_rtv, clearColor);
            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);

            // 3. 렌더링 파이프라인 설정
            UINT stride = sizeof(Vertex), offset = 0;
            g_context->IASetVertexBuffers(0, 1, &g_vbo, &stride, &offset);
            g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_context->IASetInputLayout(g_layout);
            g_context->VSSetShader(g_vs, nullptr, 0);
            g_context->PSSetShader(g_ps, nullptr, 0);

            // 4. 버튼 렌더링 (상수 버퍼 업데이트 및 바인딩)
            btn.Render(g_context);

            // 5. 그리기 명령 실행 및 화면 출력
            g_context->Draw(6, 0);
            g_swapChain->Present(1, 0);
        }
    }

    // 리소스 해제
    if (g_vbo) g_vbo->Release();
    if (g_layout) g_layout->Release();
    if (g_vs) g_vs->Release();
    if (g_ps) g_ps->Release();
    if (g_rtv) g_rtv->Release();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();

    return (int)msg.wParam;
}