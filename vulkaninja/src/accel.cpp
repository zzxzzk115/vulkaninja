#include "vulkaninja/accel.hpp"

#include "vulkaninja/command_buffer.hpp"
#include "vulkaninja/common.hpp"

namespace vulkaninja
{
    BottomAccel::BottomAccel(const Context& context, const BottomAccelCreateInfo& createInfo) :
        m_Context {&context}, m_GeometryFlags {createInfo.geometryFlags}, m_BuildFlags {createInfo.buildFlags},
        m_BuildType {createInfo.buildType}
    {
        m_TrianglesData.setVertexFormat(vk::Format::eR32G32B32Sfloat);
        m_TrianglesData.setVertexData(createInfo.vertexBuffer->getAddress());
        m_TrianglesData.setVertexStride(createInfo.vertexStride);
        m_TrianglesData.setMaxVertex(createInfo.maxVertexCount);
        m_TrianglesData.setIndexType(vk::IndexTypeValue<uint32_t>::value);
        m_TrianglesData.setIndexData(createInfo.indexBuffer->getAddress());

        vk::AccelerationStructureGeometryDataKHR geometryData;
        geometryData.setTriangles(m_TrianglesData);

        vk::AccelerationStructureGeometryKHR geometry;
        geometry.setGeometryType(vk::GeometryTypeKHR::eTriangles);
        geometry.setGeometry({geometryData});
        geometry.setFlags(m_GeometryFlags);

        vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
        buildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
        buildGeometryInfo.setFlags(m_BuildFlags);
        buildGeometryInfo.setGeometries(geometry);

        m_PrimitiveCount    = createInfo.triangleCount;
        m_MaxPrimitiveCount = createInfo.maxTriangleCount;
        auto buildSizesInfo = m_Context->getDevice().getAccelerationStructureBuildSizesKHR(
            m_BuildType, buildGeometryInfo, m_MaxPrimitiveCount);

        m_Buffer = m_Context->createBuffer({
            .usage  = BufferUsage::AccelStorage,
            .memory = MemoryUsage::Device,
            .size   = buildSizesInfo.accelerationStructureSize,
        });

        m_Accel = m_Context->getDevice().createAccelerationStructureKHRUnique(
            vk::AccelerationStructureCreateInfoKHR {}
                .setBuffer(m_Buffer->getBuffer())
                .setSize(buildSizesInfo.accelerationStructureSize)
                .setType(vk::AccelerationStructureTypeKHR::eBottomLevel));

        m_ScratchBuffer = m_Context->createBuffer({
            .usage  = BufferUsage::Scratch,
            .memory = MemoryUsage::Host,
            .size   = buildSizesInfo.buildScratchSize,
        });
    }

    TopAccel::TopAccel(const Context& context, const TopAccelCreateInfo& createInfo) :
        m_Context {&context}, m_GeometryFlags {createInfo.geometryFlags}, m_BuildFlags {createInfo.buildFlags},
        m_BuildType {createInfo.buildType}
    {
        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        for (const auto& instance : createInfo.accelInstances)
        {
            vk::AccelerationStructureInstanceKHR inst;
            inst.setTransform(toVkMatrix(instance.transform));
            inst.setInstanceCustomIndex(instance.customIndex);
            inst.setMask(0xFF);
            inst.setInstanceShaderBindingTableRecordOffset(instance.sbtOffset);
            inst.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);
            inst.setAccelerationStructureReference(instance.bottomAccel->getBufferAddress());
            instances.push_back(inst);
        }

        m_PrimitiveCount = static_cast<uint32_t>(instances.size());
        m_InstanceBuffer = m_Context->createBuffer({
            .usage  = BufferUsage::AccelInput,
            .memory = MemoryUsage::DeviceHost,
            .size   = sizeof(vk::AccelerationStructureInstanceKHR) * instances.size(),
        });
        m_InstanceBuffer->copy(instances.data());

        m_InstancesData.setArrayOfPointers(false);
        m_InstancesData.setData(m_InstanceBuffer->getAddress());

        vk::AccelerationStructureGeometryKHR geometry;
        geometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
        geometry.setGeometry({m_InstancesData});
        geometry.setFlags(m_GeometryFlags);

        vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
        buildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
        buildGeometryInfo.setFlags(m_BuildFlags);
        buildGeometryInfo.setGeometries(geometry);

        auto buildSizesInfo = m_Context->getDevice().getAccelerationStructureBuildSizesKHR(
            m_BuildType, buildGeometryInfo, static_cast<uint32_t>(instances.size()));

        m_Buffer = m_Context->createBuffer({
            .usage  = BufferUsage::AccelStorage,
            .memory = MemoryUsage::Device,
            .size   = buildSizesInfo.accelerationStructureSize,
        });

        m_Accel = m_Context->getDevice().createAccelerationStructureKHRUnique(
            vk::AccelerationStructureCreateInfoKHR {}
                .setBuffer(m_Buffer->getBuffer())
                .setSize(buildSizesInfo.accelerationStructureSize)
                .setType(vk::AccelerationStructureTypeKHR::eTopLevel));

        m_ScratchBuffer = m_Context->createBuffer({
            .usage  = BufferUsage::Scratch,
            .memory = MemoryUsage::Device,
            .size   = buildSizesInfo.buildScratchSize,
        });
    }

    void BottomAccel::update(const BufferHandle& vertexBuffer, const BufferHandle& indexBuffer, uint32_t triangleCount)
    {
        assert(triangleCount <= m_MaxPrimitiveCount);

        m_TrianglesData.setVertexData(vertexBuffer->getAddress());
        m_TrianglesData.setIndexData(indexBuffer->getAddress());
        m_LastPrimitiveCount = m_PrimitiveCount;
        m_PrimitiveCount     = triangleCount;
    }

    void TopAccel::updateInstances(ArrayProxy<AccelInstance> accelInstances) const
    {
        VKN_ASSERT(m_PrimitiveCount == accelInstances.size(),
                   "Instance count was changed. {} == {}",
                   m_PrimitiveCount,
                   accelInstances.size());

        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        for (const auto& instance : accelInstances)
        {
            vk::AccelerationStructureInstanceKHR inst;
            inst.setTransform(toVkMatrix(instance.transform));
            inst.setInstanceCustomIndex(instance.customIndex);
            inst.setMask(0xFF);
            inst.setInstanceShaderBindingTableRecordOffset(instance.sbtOffset);
            inst.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);
            inst.setAccelerationStructureReference(instance.bottomAccel->getBufferAddress());
            instances.push_back(inst);
        }

        // TODO: use CommandBuffer::copy()
        m_InstanceBuffer->copy(instances.data());
    }
} // namespace vulkaninja