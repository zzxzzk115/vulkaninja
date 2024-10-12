#include "vulkaninja/shader.hpp"

namespace vulkaninja
{
    Shader::Shader(const Context& context, const ShaderCreateInfo& createInfo) :
        m_SpvCode(createInfo.code), m_Stage(createInfo.stage)
    {
        vk::ShaderModuleCreateInfo moduleInfo;
        moduleInfo.setCode(m_SpvCode);
        m_ShaderModule = context.getDevice().createShaderModuleUnique(moduleInfo);
    }
} // namespace vulkaninja