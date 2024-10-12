#pragma once

#include "vulkaninja/array_proxy.hpp"
#include "vulkaninja/image.hpp"
#include "vulkaninja/shader.hpp"

#include <spirv_cross/spirv_glsl.hpp>

#include <unordered_map>
#include <variant>

namespace vulkaninja
{
    struct DescriptorSetCreateInfo
    {
        ArrayProxy<ShaderHandle>                                                               shaders;
        ArrayProxy<std::pair<const char*, std::variant<ArrayProxy<BufferHandle>, uint32_t>>>   buffers;
        ArrayProxy<std::pair<const char*, std::variant<ArrayProxy<ImageHandle>, uint32_t>>>    images;
        ArrayProxy<std::pair<const char*, std::variant<ArrayProxy<TopAccelHandle>, uint32_t>>> accels;
    };

    class DescriptorSet
    {
    public:
        DescriptorSet(const Context& context, const DescriptorSetCreateInfo& createInfo);

        void update();

        void set(const std::string& name, ArrayProxy<BufferHandle> buffers);
        void set(const std::string& name, ArrayProxy<ImageHandle> images);
        void set(const std::string& name, ArrayProxy<TopAccelHandle> accels);

        vk::DescriptorSetLayout getLayout() const { return *m_DescSetLayout; }
        vk::DescriptorSet       getDescriptorSet() const { return *m_DescSet; }

    private:
        void addResources(ShaderHandle shader);
        void updateBindingMap(const spirv_cross::Resource&     resource,
                              const spirv_cross::CompilerGLSL& glsl,
                              vk::ShaderStageFlags             stage,
                              vk::DescriptorType               type);

        const Context*                m_Context;
        vk::UniqueDescriptorSet       m_DescSet;
        vk::UniqueDescriptorSetLayout m_DescSetLayout;

        using BufferInfos = std::vector<vk::DescriptorBufferInfo>;
        using ImageInfos  = std::vector<vk::DescriptorImageInfo>;
        using AccelInfos  = std::vector<vk::WriteDescriptorSetAccelerationStructureKHR>;
        struct Descriptor
        {
            vk::DescriptorSetLayoutBinding                    binding;
            std::variant<BufferInfos, ImageInfos, AccelInfos> infos;
        };

        std::unordered_map<std::string, Descriptor> m_Descriptors;
    };
} // namespace vulkaninja