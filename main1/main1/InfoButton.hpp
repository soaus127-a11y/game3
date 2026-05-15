#pragma once
#include "ButtonFSM.hpp"
#include "Logger.hpp"
#include <d3d11.h>
#include <directxmath.h>

// GPU(쉐이더)로 전달할 데이터 구조체 (16바이트 정렬 주의)
struct ConstantBuffer {
    DirectX::XMFLOAT4 color; // 버튼의 최종 렌더링 색상
};

class InfoButton : public ButtonFSM {
private:
    float x, y, w, h;       // 윈도우 상의 버튼 좌표와 크기
    int clickCount = 0;     // 클릭 횟수 관리 (색상 순환용)
    DirectX::XMFLOAT4 currentColor = { 0.5f, 0.5f, 0.5f, 1.0f }; // 현재 색상 (초기: 회색)
    ID3D11Buffer* cb = nullptr; // GPU에 색상 정보를 보낼 상수 버퍼 객체

public:
    InfoButton(ID3D11Device* device, float _x, float _y, float _w, float _h)
        : x(_x), y(_y), w(_w), h(_h) {

        // 상수 버퍼 리소스 생성 설명: CPU의 색상값을 GPU 쉐이더 메모리로 복사하기 위함
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(ConstantBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        device->CreateBuffer(&bd, nullptr, &cb);
    }

    // 매 프레임 마우스 입력에 따라 상태를 판정하는 핵심 로직
    void Update(int mx, int my, bool isLDown) {
        bool isInside = (mx >= x && mx <= x + w && my >= y && my <= y + h);
        eButtonState nextState = currentState;

        // FSM 상태 전이도 설계
        if (currentState == eButtonState::IDLE && isInside) nextState = eButtonState::HOVER;
        else if (currentState == eButtonState::HOVER) {
            if (!isInside) nextState = eButtonState::IDLE;
            else if (isLDown) nextState = eButtonState::PRESSED;
        }
        else if (currentState == eButtonState::PRESSED && !isLDown) {
            nextState = isInside ? eButtonState::RELEASED : eButtonState::IDLE;
        }
        else if (currentState == eButtonState::RELEASED) {
            nextState = isInside ? eButtonState::HOVER : eButtonState::IDLE;
        }

        if (nextState != currentState) OnStateChanged(nextState);
    }

    // 상태가 변했을 때 실행될 이벤트 함수
    void OnStateChanged(eButtonState newState) {
        const char* stateNames[] = { "IDLE", "HOVER", "PRESSED", "RELEASED" };
        Logger::Get()->Log("[ButtonFSM] State Changed: %s -> %s", stateNames[(int)currentState], stateNames[(int)newState]);

        ChangeState(newState);

        // 클릭 완료 시 색상 로직 처리
        if (newState == eButtonState::RELEASED) {
            clickCount = (clickCount + 1) % 3;
            Logger::Get()->Log("--- Info Button Clicked ---");
            if (clickCount == 1) {
                currentColor = { 1.0f, 1.0f, 0.0f, 1.0f }; // 노란색
                Logger::Get()->Log("[INFO] State: CLICKED_1 | Message: 버튼이 노란색으로 변경되었습니다.");
            }
            else if (clickCount == 2) {
                currentColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // 빨간색
                Logger::Get()->Log("[INFO] State: CLICKED_2 | Message: 버튼이 빨간색으로 변경되었습니다.");
            }
            else {
                currentColor = { 0.5f, 0.5f, 0.5f, 1.0f }; // 기본 회색
                Logger::Get()->Log("[INFO] State: IDLE | Message: 상태가 초기화되었습니다.");
            }
            Logger::Get()->Log("---------------------------");
        }
        else if (newState == eButtonState::PRESSED) {
            currentColor = { 0.0f, 0.0f, 1.0f, 1.0f }; // 누르는 동안은 파란색
        }
    }

    // GPU의 상수 버퍼에 현재 색상을 업데이트
    void Render(ID3D11DeviceContext* context) {
        ConstantBuffer data = { currentColor };
        context->UpdateSubresource(cb, 0, nullptr, &data, 0, 0);
        context->PSSetConstantBuffers(0, 1, &cb); // 픽셀 쉐이더 0번 슬롯에 바인딩
    }

    ~InfoButton() { if (cb) cb->Release(); }
};