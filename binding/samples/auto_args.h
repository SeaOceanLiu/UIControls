// Sample 自动测试统一命令行方案：auto=<秒>
// 与标准测试（test/TestInstance.h scheduleAutoQuit）对齐：任意顺序识别
// "auto=<秒>"，达到时长后主循环自行退出；未识别参数打印 WARN 忽略。
// 用法：
//     uicorn_sample::AutoTimer autoTimer;
//     int autoSeconds = autoTimer.parse(argc, argv, extraPrefixes, extraCount);
//     ...主循环每帧...
//     if (autoTimer.expired()) break;   // 人工模式永不触发
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

namespace uicorn_sample {

class AutoTimer {
public:
    // 解析命令行：识别 "auto=<秒>"（0 = 人工模式，不自动退出）。
    // extraPrefixes：调用方额外的合法前缀（如 sample_scale 的 "backend="），
    // 其余参数打印 WARN 后忽略。返回自动秒数。
    int parse(int argc, char* argv[], const char* const* extraPrefixes = nullptr, int extraCount = 0) {
        int sec = 0;
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "auto=", 5) == 0) {
                sec = atoi(argv[i] + 5);
                continue;
            }
            bool known = false;
            for (int k = 0; k < extraCount; k++) {
                if (strncmp(argv[i], extraPrefixes[k], std::strlen(extraPrefixes[k])) == 0) {
                    known = true;
                    break;
                }
            }
            if (!known) std::printf("WARN: 忽略无法识别的参数: %s\n", argv[i]);
        }
        m_seconds = sec;
        return sec;
    }

    // 自动模式是否已到时长（人工模式恒为 false）。
    // 计时起点为首次检查时刻（主循环第一帧，窗口/后端已创建完毕），
    // 避免把窗口创建等初始化耗时计入自动时长。
    bool expired() const {
        if (m_seconds <= 0) return false;
        using Clock = std::chrono::steady_clock;
        if (m_start == Clock::time_point{}) {
            m_start = Clock::now();
            return false;
        }
        return Clock::now() - m_start >= std::chrono::seconds(m_seconds);
    }

private:
    int m_seconds = 0;
    mutable std::chrono::steady_clock::time_point m_start{};
};

} // namespace uicorn_sample
