#pragma once

#include "vulkaninja/context.hpp"

namespace vulkaninja
{
    struct ShaderCreateInfo
    {
        const std::vector<uint32_t>& code;
        vk::ShaderStageFlagBits      stage;
    };

    class Shader
    {
    public:
        Shader(const Context& context, const ShaderCreateInfo& createInfo);

        auto getSpvCode() const { return m_SpvCode; }
        auto getModule() const { return *m_ShaderModule; }
        auto getStage() const { return m_Stage; }

    private:
        vk::UniqueShaderModule  m_ShaderModule;
        vk::UniqueShaderEXT     m_Shader;
        std::vector<uint32_t>   m_SpvCode;
        vk::ShaderStageFlagBits m_Stage;
    };
} // namespace vulkaninja