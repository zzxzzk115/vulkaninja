add_requires("spdlog", "stb", "shaderc", "spirv-cross", "vulkansdk", "vulkan-hpp")

if has_config("enable_extension") then
    add_requires("glfw")
    add_requires("imgui v1.90.9-docking", {configs = {glfw = true, vulkan = true, wchar32 = true}})
end

-- target defination, name: vulkaninja
target("vulkaninja")
    set_kind(get_config("kind") or "static")

    add_includedirs("include", { public = true })

    -- add header & source files
    add_headerfiles("include/(vulkaninja/**.hpp)")
    add_files("src/**.cpp")

    if is_kind("shared") then 
        add_rules("utils.symbols.export_all")
    end

    add_packages("spdlog", "stb", "shaderc", "spirv-cross", "vulkansdk", "vulkan-hpp", { public = true })
    
    if has_config("enable_extension") then
        add_packages("glfw", "imgui", { public = true })
        add_defines("VKN_ENABLE_EXTENSION")
    end

    add_defines("VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1")