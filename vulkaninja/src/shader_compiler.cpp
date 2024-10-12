#include "vulkaninja/shader_compiler.hpp"
#include "shaderc/shaderc.h"

#include <shaderc/shaderc.hpp>

#include <fstream>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace
{
    auto readAllText(const std::string& path) -> std::string
    {
        std::ifstream in(path);

        if (in.bad())
        {
            const std::string message = "Failed to read file: " + path;
            throw std::runtime_error(message.c_str());
        }

        std::stringstream ss;
        ss << in.rdbuf();

        return static_cast<std::string>(ss.str());
    }

    auto shaderStageToShadercKind(vulkaninja::ShaderCompiler::ShaderStage stage) -> shaderc_shader_kind
    {
        switch (stage)
        {
            case vulkaninja::ShaderCompiler::ShaderStage::eVertex:
                return shaderc_vertex_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eGeometry:
                return shaderc_geometry_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eFragment:
                return shaderc_fragment_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eCompute:
                return shaderc_compute_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eTessControl:
                return shaderc_tess_control_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eTessEvaluation:
                return shaderc_tess_evaluation_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eRayGen:
                return shaderc_raygen_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eAnyHit:
                return shaderc_anyhit_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eClosestHit:
                return shaderc_closesthit_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eMiss:
                return shaderc_miss_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eIntersection:
                return shaderc_intersection_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eCallable:
                return shaderc_callable_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eTask:
                return shaderc_task_shader;
            case vulkaninja::ShaderCompiler::ShaderStage::eMesh:
                return shaderc_mesh_shader;
        }

        // fallback
        return shaderc_vertex_shader;
    }
} // namespace

namespace vulkaninja
{
    namespace ShaderCompiler
    {
        // NOLINTBEGIN
        class MyIncluder : public shaderc::CompileOptions::IncluderInterface
        {
        public:
            MyIncluder() : search_paths_({"assets/shaders"}) {}

            MyIncluder(const std::vector<std::filesystem::path>& search_paths) : search_paths_(search_paths)
            {
                // default search paths
                search_paths_.emplace_back("assets/shaders");
            }

            virtual shaderc_include_result* GetInclude(const char*          requested_source,
                                                       shaderc_include_type type,
                                                       const char*          requesting_source,
                                                       size_t               include_depth) override final
            {
                for (const auto& search_path : search_paths_)
                {
                    std::string   file_path = (search_path / requested_source).generic_string();
                    std::ifstream file_stream(file_path.c_str(), std::ios::binary);
                    if (file_stream.is_open())
                    {
                        FileInfo*         new_file_info = new FileInfo {file_path, {}};
                        std::vector<char> file_content((std::istreambuf_iterator<char>(file_stream)),
                                                       std::istreambuf_iterator<char>());
                        new_file_info->contents = file_content;
                        return new shaderc_include_result {new_file_info->path.data(),
                                                           new_file_info->path.length(),
                                                           new_file_info->contents.data(),
                                                           new_file_info->contents.size(),
                                                           new_file_info};
                    }
                }

                return nullptr;
            }

            virtual void ReleaseInclude(shaderc_include_result* data) override final
            {
                FileInfo* info = static_cast<FileInfo*>(data->user_data);
                delete info;
                delete data;
            }

        private:
            struct FileInfo
            {
                const std::string path;
                std::vector<char> contents;
            };

            std::unordered_set<std::string>    included_files_;
            std::vector<std::filesystem::path> search_paths_;
        };
        // NOLINTEND

        auto compileShaderFromFile(const std::filesystem::path& filepath,
                                   ShaderStage                  stage,
                                   const std::string&           entrypoint,
                                   std::vector<uint32_t>&       spv,
                                   std::string&                 message) -> bool
        {
            const std::string src      = readAllText(filepath.generic_string());
            const std::string filename = filepath.filename().generic_string();
            return compileShaderFromSource(src, stage, entrypoint, filename, spv, message);
        }

        auto compileShaderFromFile(
            const std::filesystem::path&                                                        filepath,
            ShaderStage                                                                         stage,
            const std::string&                                                                  entrypoint,
            const std::vector<std::variant<std::string, std::tuple<std::string, std::string>>>& keywords,
            std::vector<uint32_t>&                                                              spv,
            std::string&                                                                        message) -> bool
        {
            const std::string           src        = readAllText(filepath.generic_string());
            const std::string           filename   = filepath.filename().generic_string();
            const std::filesystem::path searchPath = filepath.parent_path();

            shaderc::Compiler       compiler;
            shaderc::CompileOptions options;
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
            // options.SetOptimizationLevel(shaderc_optimization_level_performance);
            // options.SetAutoMapLocations(false);
            // options.SetAutoBindUniforms(false);
            const std::vector<std::filesystem::path> searchPaths = {searchPath};
            options.SetIncluder(std::make_unique<MyIncluder>(searchPaths));

            // Setup keywords
            for (const auto& keyword : keywords)
            {
                if (std::holds_alternative<std::string>(keyword))
                {
                    options.AddMacroDefinition(std::get<std::string>(keyword));
                }
                else if (std::holds_alternative<std::tuple<std::string, std::string>>(keyword))
                {
                    auto [key, value] = std::get<std::tuple<std::string, std::string>>(keyword);
                    options.AddMacroDefinition(key, value);
                }
            }

            // Preprocess
            auto preResult = compiler.PreprocessGlsl(src, shaderStageToShadercKind(stage), filename.c_str(), options);
            if (preResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                message = preResult.GetErrorMessage();
                return false;
            }

            std::string prePassesString(preResult.begin());

            // Compile
            auto compileResult = compiler.CompileGlslToSpv(
                prePassesString, shaderStageToShadercKind(stage), filename.c_str(), entrypoint.c_str(), options);
            if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                auto innerErrorMsg = compileResult.GetErrorMessage();
                message            = "Inner Message: " + innerErrorMsg + ", Preprocessed source: " + prePassesString;
                return false;
            }

            spv = std::vector<uint32_t>(compileResult.cbegin(), compileResult.cend());
            return true;
        }

        auto compileShaderFromSource(const std::string&     src,
                                     ShaderStage            stage,
                                     const std::string&     entrypoint,
                                     const std::string&     name,
                                     std::vector<uint32_t>& spv,
                                     std::string&           message) -> bool
        {
            return compileShaderFromSource(src, stage, entrypoint, name, {}, spv, message);
        }

        auto compileShaderFromSource(
            const std::string&                                                                  src,
            ShaderStage                                                                         stage,
            const std::string&                                                                  entrypoint,
            const std::string&                                                                  name,
            const std::vector<std::variant<std::string, std::tuple<std::string, std::string>>>& keywords,
            std::vector<uint32_t>&                                                              spv,
            std::string&                                                                        message) -> bool
        {
            shaderc::Compiler       compiler;
            shaderc::CompileOptions options;
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
            // options.SetOptimizationLevel(shaderc_optimization_level_performance);
            // options.SetAutoMapLocations(false);
            // options.SetAutoBindUniforms(false);
            options.SetIncluder(std::make_unique<MyIncluder>());

            // Setup keywords
            for (const auto& keyword : keywords)
            {
                if (std::holds_alternative<std::string>(keyword))
                {
                    options.AddMacroDefinition(std::get<std::string>(keyword));
                }
                else if (std::holds_alternative<std::tuple<std::string, std::string>>(keyword))
                {
                    auto [key, value] = std::get<std::tuple<std::string, std::string>>(keyword);
                    options.AddMacroDefinition(key, value);
                }
            }

            // Preprocess
            auto preResult = compiler.PreprocessGlsl(src, shaderStageToShadercKind(stage), name.c_str(), options);
            if (preResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                message = preResult.GetErrorMessage();
                return false;
            }

            std::string prePassesString(preResult.begin());

            // Compile
            auto compileResult = compiler.CompileGlslToSpv(
                prePassesString, shaderStageToShadercKind(stage), name.c_str(), entrypoint.c_str(), options);
            if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                auto innerErrorMsg = compileResult.GetErrorMessage();
                message            = "Inner Message: " + innerErrorMsg + ", Preprocessed source: " + prePassesString;
                return false;
            }

            spv = std::vector<uint32_t>(compileResult.cbegin(), compileResult.cend());
            return true;
        }
    } // namespace ShaderCompiler
} // namespace vulkaninja