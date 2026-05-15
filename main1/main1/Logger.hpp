#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string>

// 로그의 중요도를 구분하기 위한 열거형
enum class LogLevel { Info, Warning, Error };

class Logger {
private:
    FILE* file;
    // C++17 inline static: 여러 파일에서 참조해도 메모리 중복 생성을 방지함
    inline static Logger* instance = nullptr;
    Logger() : file(nullptr) {}

public:
    // 싱글톤 패턴: 어디서든 Logger::Get()으로 동일한 객체에 접근
    static Logger* Get() {
        if (!instance) instance = new Logger();
        return instance;
    }

    // 시스템 초기화: 콘솔 인코딩 설정 및 로그 파일 생성
    bool Initialize(const std::string& filename) {
        system("chcp 65001"); // 콘솔 출력 한글 깨짐 방지 (UTF-8)
        file = fopen(filename.c_str(), "w");
        return file != nullptr;
    }

    // 가변 인자 함수: printf처럼 포맷팅된 문자열을 입력받음
    void Log(const char* format, ...) {
        // [1] 타임스탬프 생성 (예: Fri May 15 21:05:30 2026)
        time_t now = time(0);
        char* dt = ctime(&now);
        std::string ts = dt;
        ts = ts.substr(0, ts.length() - 1); // ctime의 끝에 붙는 줄바꿈 제거

        // [2] 가변 인자 처리 (사용자가 보낸 메시지 조립)
        va_list args;
        va_start(args, format);
        char messageBuf[1024];
        vsnprintf(messageBuf, sizeof(messageBuf), format, args);
        va_end(args);

        // [3] 최종 출력: 콘솔창과 로그 파일에 동시에 기록
        printf("[%s] %s\n", ts.c_str(), messageBuf);
        if (file) {
            fprintf(file, "[%s] %s\n", ts.c_str(), messageBuf);
            fflush(file); // 즉시 파일에 쓰기 (프로그램 종료 시 손실 방지)
        }
    }

    ~Logger() { if (file) fclose(file); }
};