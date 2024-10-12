#include "vulkaninja/fence.hpp"
#include "vulkaninja/context.hpp"

namespace vulkaninja
{
    Fence::Fence(const Context& context, const FenceCreateInfo& createInfo) : m_Context(&context)
    {
        vk::FenceCreateInfo fenceInfo;
        if (createInfo.signaled)
        {
            fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
        }
        m_Fence = m_Context->getDevice().createFenceUnique(fenceInfo);
    }

    void Fence::wait() const
    {
        if (m_Context->getDevice().waitForFences(*m_Fence, VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to wait for fence");
        }
    }

    void Fence::reset() const { m_Context->getDevice().resetFences(*m_Fence); }

    auto Fence::finished() const -> bool
    {
        return m_Context->getDevice().getFenceStatus(*m_Fence) == vk::Result::eSuccess;
    }
} // namespace vulkaninja