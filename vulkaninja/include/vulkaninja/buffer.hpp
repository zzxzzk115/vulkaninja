#pragma once

#include "vulkaninja/context.hpp"

namespace vulkaninja
{
    struct BufferCreateInfo
    {
        vk::BufferUsageFlags usage;

        vk::MemoryPropertyFlags memory;

        size_t size = 0;

        std::string debugName;
    };

    class Buffer
    {
        friend class CommandBuffer;

    public:
        Buffer(const Context& context, const BufferCreateInfo& createInfo);

        auto getBuffer() const -> vk::Buffer { return *m_Buffer; }
        auto getSize() const -> vk::DeviceSize { return m_Size; }
        auto getInfo() const -> vk::DescriptorBufferInfo { return {*m_Buffer, 0, m_Size}; }
        auto getAddress() const -> vk::DeviceAddress;

        auto map() -> void*;
        void unmap();
        void copy(const void* data);

        void prepareStagingBuffer();

    private:
        const Context* m_Context = nullptr;

        vk::UniqueBuffer       m_Buffer;
        vk::UniqueDeviceMemory m_Memory;
        vk::DeviceSize         m_Size = 0u;

        // For host buffer
        void* m_Mapped = nullptr;
        bool  m_IsHostVisible;

        // For device buffer
        BufferHandle m_StagingBuffer;
    };
} // namespace vulkaninja