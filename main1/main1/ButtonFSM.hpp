#pragma once

// 버튼의 4가지 핵심 상태 정의
enum class eButtonState {
    IDLE,       // 기본 상태 (마우스가 영역 밖에 있음)
    HOVER,      // 마우스가 버튼 영역 위에 올라와 있음
    PRESSED,    // 마우스 왼쪽 버튼을 누르고 있는 상태
    RELEASED    // 누르고 있던 버튼을 떼서 클릭이 완료된 상태
};

class ButtonFSM {
protected:
    eButtonState currentState = eButtonState::IDLE;

public:
    virtual ~ButtonFSM() {}

    // 상태 변경 메서드: 자식 클래스에서 이 값을 기준으로 행동(색상 변경 등) 결정
    void ChangeState(eButtonState newState) { currentState = newState; }
    eButtonState GetState() const { return currentState; }
};