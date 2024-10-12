#pragma once

#include "vulkaninja/context.hpp"

namespace vulkaninja
{
    struct GPUTimerCreateInfo
    {};

    class GPUTimer
    {
        friend class CommandBuffer;

        enum class State
        {
            eReady,
            eStarted,
            eStopped,
        };

    public:
        GPUTimer() = default;
        GPUTimer(const Context& context, const GPUTimerCreateInfo& createInfo);

        auto elapsedInNano() -> float;
        auto elapsedInMilli() -> float;

        void start() { m_State = State::eStarted; }
        void stop() { m_State = State::eStopped; }

    private:
        const Context* m_Context = nullptr;

        float m_TimestampPeriod;

        vk::UniqueQueryPool m_QueryPool;

        std::array<uint64_t, 2> m_Timestamps {};

        State m_State;
    };
} // namespace vulkaninja