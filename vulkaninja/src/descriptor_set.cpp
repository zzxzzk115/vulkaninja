#include "vulkaninja/descriptor_set.hpp"
#include "vulkaninja/accel.hpp"
#include "vulkaninja/buffer.hpp"

#include <ranges>
#include <stdexcept>
#include <vector>

namespace vulkaninja
{
    DescriptorSet::DescriptorSet(const Context& context, const DescriptorSetCreateInfo& createInfo) :
        m_Context {&context}
    {
        for (const auto& shader : createInfo.shaders)
        {
            addResources(shader);
        }

        for (const auto& [name, buffers] : createInfo.buffers)
        {
            if (std::holds_alternative<uint32_t>(buffers))
            {
                m_Descriptors[name].binding.setDescriptorCount(std::get<uint32_t>(buffers));
            }
            else
            {
                set(name, std::get<ArrayProxy<BufferHandle>>(buffers));
            }
        }
        for (const auto& [name, images] : createInfo.images)
        {
            if (std::holds_alternative<uint32_t>(images))
            {
                m_Descriptors[name].binding.setDescriptorCount(std::get<uint32_t>(images));
            }
            else
            {
                set(name, std::get<ArrayProxy<ImageHandle>>(images));
            }
        }
        for (const auto& [name, accels] : createInfo.accels)
        {
            if (std::holds_alternative<uint32_t>(accels))
            {
                m_Descriptors[name].binding.setDescriptorCount(std::get<uint32_t>(accels));
            }
            else
            {
                set(name, std::get<ArrayProxy<TopAccelHandle>>(accels));
            }
        }

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        for (const auto& descriptor : m_Descriptors | std::views::values)
        {
            bindings.push_back(descriptor.binding);
        }

        vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
        m_DescSetLayout = m_Context->getDevice().createDescriptorSetLayoutUnique(layoutInfo);

        vk::DescriptorSetAllocateInfo allocInfo(m_Context->getDescriptorPool(), *m_DescSetLayout);
        m_DescSet = std::move(m_Context->getDevice().allocateDescriptorSetsUnique(allocInfo).front());
    }

    void DescriptorSet::update()
    {
        std::vector<vk::WriteDescriptorSet> descriptorWrites;

        for (const auto& [binding, infos] : m_Descriptors | std::views::values)
        {
            if (std::holds_alternative<BufferInfos>(infos))
            {
                const auto&            bufferInfos = std::get<BufferInfos>(infos);
                vk::WriteDescriptorSet descriptorWrite;
                descriptorWrite.setBufferInfo(bufferInfos);
                descriptorWrite.setDescriptorCount(static_cast<uint32_t>(bufferInfos.size()));
                descriptorWrite.setDescriptorType(binding.descriptorType);
                descriptorWrite.setDstBinding(binding.binding);
                descriptorWrite.setDstSet(*m_DescSet);
                descriptorWrites.push_back(descriptorWrite);
            }
            else if (std::holds_alternative<ImageInfos>(infos))
            {
                const auto&            imageInfos = std::get<ImageInfos>(infos);
                vk::WriteDescriptorSet descriptorWrite;
                descriptorWrite.setImageInfo(imageInfos);
                descriptorWrite.setDescriptorCount(static_cast<uint32_t>(imageInfos.size()));
                descriptorWrite.setDescriptorType(binding.descriptorType);
                descriptorWrite.setDstBinding(binding.binding);
                descriptorWrite.setDstSet(*m_DescSet);
                descriptorWrites.push_back(descriptorWrite);
            }
            else if (std::holds_alternative<AccelInfos>(infos))
            {
                const auto&            accelInfos = std::get<AccelInfos>(infos);
                vk::WriteDescriptorSet descriptorWrite;
                descriptorWrite.setDescriptorCount(static_cast<uint32_t>(accelInfos.size()));
                descriptorWrite.setDescriptorType(binding.descriptorType);
                descriptorWrite.setDstBinding(binding.binding);
                descriptorWrite.setDstSet(*m_DescSet);
                descriptorWrite.setPNext(accelInfos.data());
                descriptorWrites.push_back(descriptorWrite);
            }
        }

        m_Context->getDevice().updateDescriptorSets(descriptorWrites, nullptr);
    }

    void DescriptorSet::set(const std::string& name, ArrayProxy<BufferHandle> buffers)
    {
        std::vector<vk::DescriptorBufferInfo> bufferInfos;
        for (const auto& buffer : buffers)
        {
            bufferInfos.push_back(buffer->getInfo());
        }
        m_Descriptors[name].binding.descriptorCount = buffers.size();
        m_Descriptors[name].infos                   = bufferInfos;
    }

    void DescriptorSet::set(const std::string& name, ArrayProxy<ImageHandle> images)
    {
        ImageInfos imageInfos;
        for (const auto& image : images)
        {
            imageInfos.push_back(image->getInfo());
        }
        m_Descriptors[name].binding.descriptorCount = images.size();
        m_Descriptors[name].infos                   = imageInfos;
    }

    void DescriptorSet::set(const std::string& name, ArrayProxy<TopAccelHandle> accels)
    {
        AccelInfos accelInfos;
        for (const auto& accel : accels)
        {
            accelInfos.push_back(accel->getInfo());
        }
        m_Descriptors[name].binding.descriptorCount = accels.size();
        m_Descriptors[name].infos                   = accelInfos;
    }

    void DescriptorSet::addResources(ShaderHandle shader)
    {
        vk::ShaderStageFlags                stage = shader->getStage();
        spirv_cross::CompilerGLSL           glsl {shader->getSpvCode()};
        const spirv_cross::ShaderResources& resources = glsl.get_shader_resources();

        for (const auto& resource : resources.uniform_buffers)
        {
            updateBindingMap(resource, glsl, stage, vk::DescriptorType::eUniformBuffer);
        }
        for (const auto& resource : resources.acceleration_structures)
        {
            updateBindingMap(resource, glsl, stage, vk::DescriptorType::eAccelerationStructureKHR);
        }
        for (const auto& resource : resources.storage_buffers)
        {
            updateBindingMap(resource, glsl, stage, vk::DescriptorType::eStorageBuffer);
        }
        for (const auto& resource : resources.storage_images)
        {
            updateBindingMap(resource, glsl, stage, vk::DescriptorType::eStorageImage);
        }
        for (const auto& resource : resources.sampled_images)
        {
            updateBindingMap(resource, glsl, stage, vk::DescriptorType::eCombinedImageSampler);
        }
    }

    void DescriptorSet::updateBindingMap(const spirv_cross::Resource&     resource,
                                         const spirv_cross::CompilerGLSL& glsl,
                                         vk::ShaderStageFlags             stage,
                                         vk::DescriptorType               type)
    {
        if (m_Descriptors.contains(resource.name))
        {
            auto& binding = m_Descriptors[resource.name].binding;
            if (binding.binding != glsl.get_decoration(resource.id, spv::DecorationBinding))
            {
                throw std::runtime_error("binding does not match.");
            }
            binding.stageFlags |= stage;
        }
        else
        {
            // FIX: If the count is 1 here, it cannot be increased later
            m_Descriptors[resource.name] = {
                .binding = vk::DescriptorSetLayoutBinding()
                               .setBinding(glsl.get_decoration(resource.id, spv::DecorationBinding))
                               .setDescriptorType(type)
                               .setDescriptorCount(1)
                               .setStageFlags(stage),
            };
        }
    }
} // namespace vulkaninja