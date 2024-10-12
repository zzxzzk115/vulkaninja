add_requires("shaderc")

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

    add_packages("shaderc", { public = true })