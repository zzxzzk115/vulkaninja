#pragma once

#include <chrono>

namespace vulkaninja
{
    class CPUTimer
    {
    public:
        CPUTimer() { restart(); }

        void restart() { m_StartTime = std::chrono::steady_clock::now(); }

        auto elapsedInNano() const -> float
        {
            auto endTime = std::chrono::steady_clock::now();
            return static_cast<float>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - m_StartTime).count());
        }

        auto elapsedInMilli() const -> float { return elapsedInNano() / 1000000.0f; }

    private:
        std::chrono::time_point<std::chrono::steady_clock> m_StartTime;
    };
} // namespace vulkaninja