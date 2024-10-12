#pragma once

#include "vulkaninja/array_proxy.hpp"
#include "vulkaninja/mesh.hpp"
#include "vulkaninja/shader.hpp"

#include <vulkan/vulkan.hpp>

#include <variant>

namespace vulkaninja
{
    class Image;

    struct GraphicsPipelineCreateInfo
    {
        // Layout
        vk::DescriptorSetLayout descSetLayout;

        uint32_t pushSize = 0;

        // Shader
        ShaderHandle vertexShader;
        ShaderHandle fragmentShader;

        // Vertex
        uint32_t                               vertexStride = 0;
        ArrayProxy<VertexAttributeDescription> vertexAttributes;

        // Viewport
        ArrayProxy<vk::Format> colorFormats;
        vk::Format             depthFormat;

        // Vertex input
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;

        // Raster
        std::variant<vk::PolygonMode, std::string>   polygonMode = vk::PolygonMode::eFill;
        std::variant<vk::CullModeFlags, std::string> cullMode    = vk::CullModeFlagBits::eNone;
        std::variant<vk::FrontFace, std::string>     frontFace   = vk::FrontFace::eCounterClockwise;
        std::variant<float, std::string>             lineWidth   = 1.0f;

        // Color blend
        bool alphaBlending = false;
    };

    struct ComputePipelineCreateInfo
    {
        vk::DescriptorSetLayout descSetLayout;
        uint32_t                pushSize = 0;
        ShaderHandle            computeShader;
    };

    struct MeshShaderPipelineCreateInfo
    {
        vk::DescriptorSetLayout descSetLayout;
        uint32_t                pushSize = 0;
        ShaderHandle            taskShader;
        ShaderHandle            meshShader;
        ShaderHandle            fragmentShader;

        // Viewport
        ArrayProxy<vk::Format> colorFormats;
        vk::Format             depthFormat;

        // Raster
        std::variant<vk::PolygonMode, std::string>   polygonMode = vk::PolygonMode::eFill;
        std::variant<vk::CullModeFlags, std::string> cullMode    = vk::CullModeFlagBits::eNone;
        std::variant<vk::FrontFace, std::string>     frontFace   = vk::FrontFace::eCounterClockwise;
        std::variant<float, std::string>             lineWidth   = 1.0f;

        // Color blend
        bool alphaBlending = false;
    };

    struct RaygenGroup
    {
        ShaderHandle raygenShader;
    };

    struct MissGroup
    {
        ShaderHandle missShader;
    };

    struct HitGroup
    {
        ShaderHandle chitShader;
        ShaderHandle ahitShader;
    };

    struct CallableGroup
    {
        Shader callableShader;
    };

    struct RayTracingPipelineCreateInfo
    {
        RaygenGroup               rgenGroup;
        ArrayProxy<MissGroup>     missGroups;
        ArrayProxy<HitGroup>      hitGroups;
        ArrayProxy<CallableGroup> callableGroups;

        vk::DescriptorSetLayout descSetLayout;
        uint32_t                pushSize = 0;

        uint32_t maxRayRecursionDepth = 4;
    };

    class Pipeline
    {
    public:
        explicit Pipeline(const Context& context) : m_Context {&context} {}

        auto getPipelineBindPoint() const -> vk::PipelineBindPoint { return m_BindPoint; }
        auto getPipelineLayout() const -> vk::PipelineLayout { return *m_PipelineLayout; }

    protected:
        friend class CommandBuffer;

        const Context*           m_Context = nullptr;
        vk::UniquePipelineLayout m_PipelineLayout;
        vk::UniquePipeline       m_Pipeline;
        vk::ShaderStageFlags     m_ShaderStageFlags;
        vk::PipelineBindPoint    m_BindPoint = {};
        uint32_t                 m_PushSize  = 0;
    };

    class GraphicsPipeline : public Pipeline
    {
    public:
        GraphicsPipeline(const Context& context, const GraphicsPipelineCreateInfo& createInfo);
    };

    class MeshShaderPipeline : public Pipeline
    {
    public:
        MeshShaderPipeline(const Context& context, const MeshShaderPipelineCreateInfo& createInfo);
    };

    class ComputePipeline : public Pipeline
    {
    public:
        ComputePipeline(const Context& context, const ComputePipelineCreateInfo& createInfo);
    };

    class RayTracingPipeline : public Pipeline
    {
    public:
        RayTracingPipeline() = delete;
        RayTracingPipeline(const Context& context, const RayTracingPipelineCreateInfo& createInfo);

    private:
        friend class CommandBuffer;

        void createSBT();

        std::vector<vk::ShaderModule>                       m_ShaderModules;
        std::vector<vk::PipelineShaderStageCreateInfo>      m_ShaderStages;
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> m_ShaderGroups;

        vk::StridedDeviceAddressRegionKHR m_RaygenRegion;
        vk::StridedDeviceAddressRegionKHR m_MissRegion;
        vk::StridedDeviceAddressRegionKHR m_HitRegion;
        vk::StridedDeviceAddressRegionKHR m_CallableRegion;

        BufferHandle m_SbtBuffer;

        uint32_t m_RgenCount = 0;
        uint32_t m_MissCount = 0;
        uint32_t m_HitCount  = 0;
    };
} // namespace vulkaninja