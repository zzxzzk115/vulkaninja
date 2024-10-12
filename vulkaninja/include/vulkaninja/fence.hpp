#pragma once

#include <vulkan/vulkan.hpp>

namespace vulkaninja
{
    class Context;

    struct FenceCreateInfo
    {
        bool signaled = true;
    };

    class Fence
    {
    public:
        Fence(const Context& context, const FenceCreateInfo& createInfo);

        auto getFence() const -> vk::Fence { return *m_Fence; }

        void wait() const;
        void reset() const;
        auto finished() const -> bool;

    private:
        const Context* m_Context = nullptr;

        vk::UniqueFence m_Fence;
    };
} // namespace vulkaninja