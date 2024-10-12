#include "vulkaninja/gpu_timer.hpp"

namespace vulkaninja
{
    GPUTimer::GPUTimer(const Context& context, const GPUTimerCreateInfo& /*createInfo*/) : m_Context {&context}
    {
        vk::QueryPoolCreateInfo queryPoolInfo;
        queryPoolInfo.setQueryType(vk::QueryType::eTimestamp);
        queryPoolInfo.setQueryCount(2);
        m_QueryPool       = m_Context->getDevice().createQueryPoolUnique(queryPoolInfo);
        m_TimestampPeriod = m_Context->getPhysicalDevice().getProperties().limits.timestampPeriod;
        m_State           = State::eReady;
    }

    auto GPUTimer::elapsedInNano() -> float
    {
        if (m_State != State::eStopped)
        {
            return 0.0f;
        }
        m_Timestamps.fill(0);
        if (m_Context->getDevice().getQueryPoolResults(*m_QueryPool,
                                                       0,
                                                       2,
                                                       m_Timestamps.size() * sizeof(uint64_t), // dataSize
                                                       m_Timestamps.data(),                    // pData
                                                       sizeof(uint64_t),                       // stride
                                                       vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait) !=
            vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to get query pool results");
        }
        m_State = State::eReady;

        // FIXME: Sometimes one of them is 0. There may be a problem with syncing etc.
        return m_TimestampPeriod * static_cast<float>(m_Timestamps[1] - m_Timestamps[0]);
    }

    auto GPUTimer::elapsedInMilli() -> float { return elapsedInNano() / 1000000.0f; }
} // namespace vulkaninja