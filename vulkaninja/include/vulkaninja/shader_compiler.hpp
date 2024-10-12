#pragma once

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace vulkaninja
{
    namespace ShaderCompiler
    {
        enum class ShaderStage
        {
            eVertex = 0,
            eGeometry,
            eFragment,
            eCompute,
            eTessControl,
            eTessEvaluation,
            eRayGen,
            eAnyHit,
            eClosestHit,
            eMiss,
            eIntersection,
            eCallable,
            eTask,
            eMesh,
        };

        bool compileShaderFromFile(const std::filesystem::path& filepath,
                                   ShaderStage                  stage,
                                   const std::string&           entrypoint,
                                   std::vector<uint32_t>&       spv,
                                   std::string&                 message);

        bool compileShaderFromFile(
            const std::filesystem::path&                                                        filepath,
            ShaderStage                                                                         stage,
            const std::string&                                                                  entrypoint,
            const std::vector<std::variant<std::string, std::tuple<std::string, std::string>>>& keywords,
            std::vector<uint32_t>&                                                              spv,
            std::string&                                                                        message);

        bool compileShaderFromSource(const std::string&     src,
                                     ShaderStage            stage,
                                     const std::string&     entrypoint,
                                     const std::string&     name,
                                     std::vector<uint32_t>& spv,
                                     std::string&           message);

        bool compileShaderFromSource(
            const std::string&                                                                  src,
            ShaderStage                                                                         stage,
            const std::string&                                                                  entrypoint,
            const std::string&                                                                  name,
            const std::vector<std::variant<std::string, std::tuple<std::string, std::string>>>& keywords,
            std::vector<uint32_t>&                                                              spv,
            std::string&                                                                        message);
    } // namespace ShaderCompiler
} // namespace vulkaninja