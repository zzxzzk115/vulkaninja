#include "vulkaninja/buffer.hpp"

#include "vulkaninja/command_buffer.hpp"
#include "vulkaninja/common.hpp"

namespace vulkaninja
{
    Buffer::Buffer(const Context& context, const BufferCreateInfo& createInfo) :
        m_Context {&context}, m_Size {createInfo.size}
    {
        // Create buffer
        vk::BufferCreateInfo bufferInfo;
        bufferInfo.setSize(m_Size);
        bufferInfo.setUsage(createInfo.usage);
        m_Buffer = m_Context->getDevice().createBufferUnique(bufferInfo);

        // Allocate memory
        vk::MemoryRequirements requirements    = m_Context->getDevice().getBufferMemoryRequirements(*m_Buffer);
        uint32_t               memoryTypeIndex = m_Context->findMemoryTypeIndex(requirements, createInfo.memory);

        vk::MemoryAllocateFlagsInfo flagsInfo {vk::MemoryAllocateFlagBits::eDeviceAddress};
        vk::MemoryAllocateInfo      memoryInfo;
        memoryInfo.setAllocationSize(requirements.size);
        memoryInfo.setMemoryTypeIndex(memoryTypeIndex);
        memoryInfo.setPNext(&flagsInfo);
        m_Memory = m_Context->getDevice().allocateMemoryUnique(memoryInfo);

        m_IsHostVisible = static_cast<bool>(createInfo.memory & vk::MemoryPropertyFlagBits::eHostVisible);

        // Bind memory
        m_Context->getDevice().bindBufferMemory(*m_Buffer, *m_Memory, 0);

        if (!createInfo.debugName.empty())
        {
            m_Context->setDebugName(*m_Buffer, createInfo.debugName.c_str());
            m_Context->setDebugName(*m_Memory, createInfo.debugName.c_str());
        }
    }

    auto Buffer::getAddress() const -> vk::DeviceAddress
    {
        vk::BufferDeviceAddressInfo addressInfo {*m_Buffer};
        return m_Context->getDevice().getBufferAddress(&addressInfo);
    }

    auto Buffer::map() -> void*
    {
        VKN_ASSERT(m_IsHostVisible, "");
        if (!m_Mapped)
        {
            m_Mapped = m_Context->getDevice().mapMemory(*m_Memory, 0, VK_WHOLE_SIZE);
        }
        return m_Mapped;
    }

    void Buffer::unmap()
    {
        VKN_ASSERT(m_IsHostVisible, "This buffer is not host visible.");
        m_Context->getDevice().unmapMemory(*m_Memory);
        m_Mapped = nullptr;
    }

    void Buffer::copy(const void* data)
    {
        VKN_ASSERT(m_IsHostVisible, "This buffer is not host visible.");
        map();
        std::memcpy(m_Mapped, data, m_Size);
    }

    void Buffer::prepareStagingBuffer()
    {
        VKN_ASSERT(!m_IsHostVisible, "This buffer is not device buffer.");
        if (!m_StagingBuffer)
        {
            m_StagingBuffer = m_Context->createBuffer({
                .usage  = BufferUsage::Staging,
                .memory = MemoryUsage::Host,
                .size   = m_Size,
            });
        }
    }
} // namespace vulkaninja