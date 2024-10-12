#pragma once

#include "vulkaninja/array_proxy.hpp"
#include "vulkaninja/buffer.hpp"

#include <glm/glm.hpp>

namespace vulkaninja
{
    struct BottomAccelCreateInfo
    {
        BufferHandle vertexBuffer;
        BufferHandle indexBuffer;

        uint32_t vertexStride;
        uint32_t maxVertexCount;
        uint32_t maxTriangleCount;
        uint32_t triangleCount;

        vk::GeometryFlagsKHR geometryFlags = vk::GeometryFlagBitsKHR::eOpaque;

        vk::BuildAccelerationStructureFlagsKHR buildFlags =
            vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace |
            vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

        vk::AccelerationStructureBuildTypeKHR buildType = vk::AccelerationStructureBuildTypeKHR::eDevice;
    };

    struct AccelInstance
    {
        BottomAccelHandle bottomAccel;

        glm::mat4 transform = glm::mat4 {1.0};

        uint32_t sbtOffset = 0;

        uint32_t customIndex = 0;
    };

    inline vk::TransformMatrixKHR toVkMatrix(const glm::mat4& matrix)
    {
        const glm::mat4 transposedMatrix = glm::transpose(matrix);

        vk::TransformMatrixKHR vkMatrix;
        std::memcpy(&vkMatrix, &transposedMatrix, sizeof(vk::TransformMatrixKHR));

        return vkMatrix;
    }

    struct TopAccelCreateInfo
    {
        ArrayProxy<AccelInstance> accelInstances;

        vk::GeometryFlagsKHR geometryFlags = vk::GeometryFlagBitsKHR::eOpaque;

        vk::BuildAccelerationStructureFlagsKHR buildFlags =
            vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace |
            vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

        vk::AccelerationStructureBuildTypeKHR buildType = vk::AccelerationStructureBuildTypeKHR::eDevice;
    };

    class BottomAccel
    {
        friend class CommandBuffer;

    public:
        BottomAccel(const Context& context, const BottomAccelCreateInfo& createInfo);

        auto getBufferAddress() const -> uint64_t { return m_Buffer->getAddress(); }

        void update(const BufferHandle& vertexBuffer, const BufferHandle& indexBuffer, uint32_t triangleCount);

        bool shouldRebuild() const { return m_LastPrimitiveCount != m_PrimitiveCount; }

    private:
        const Context* m_Context;

        vk::UniqueAccelerationStructureKHR m_Accel;

        BufferHandle m_Buffer;
        BufferHandle m_ScratchBuffer;

        vk::AccelerationStructureGeometryTrianglesDataKHR m_TrianglesData;
        vk::GeometryFlagsKHR                              m_GeometryFlags;
        vk::BuildAccelerationStructureFlagsKHR            m_BuildFlags;
        vk::AccelerationStructureBuildTypeKHR             m_BuildType;

        uint32_t m_MaxPrimitiveCount;
        uint32_t m_LastPrimitiveCount;
        uint32_t m_PrimitiveCount;
    };

    class TopAccel
    {
        friend class CommandBuffer;

    public:
        TopAccel(const Context& context, const TopAccelCreateInfo& createInfo);

        auto getAccel() const -> vk::AccelerationStructureKHR { return *m_Accel; }

        auto getInfo() const -> vk::WriteDescriptorSetAccelerationStructureKHR { return {*m_Accel}; }

        void updateInstances(ArrayProxy<AccelInstance> accelInstances) const;

    private:
        const Context* m_Context;

        vk::UniqueAccelerationStructureKHR m_Accel;

        BufferHandle m_Buffer;
        BufferHandle m_InstanceBuffer;
        BufferHandle m_ScratchBuffer;

        uint32_t                                          m_PrimitiveCount;
        vk::AccelerationStructureGeometryInstancesDataKHR m_InstancesData;
        vk::GeometryFlagsKHR                              m_GeometryFlags;
        vk::BuildAccelerationStructureFlagsKHR            m_BuildFlags;
        vk::AccelerationStructureBuildTypeKHR             m_BuildType;
    };
} // namespace vulkaninja