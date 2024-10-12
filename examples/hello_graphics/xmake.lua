-- target defination, name: hello_graphics
target("hello_graphics")
    -- set target kind: executable
    set_kind("binary")

    -- add source files
    add_files("main.cpp")

    -- add deps
    add_deps("vulkaninja")

    -- add rules
    add_rules("copy_assets")

    -- set values
    set_values("asset_files", "assets/**")

    -- add defines
    add_defines("VKN_ENABLE_EXTENSION")

    -- set target directory
    set_targetdir("$(buildir)/$(plat)/$(arch)/$(mode)/examples/hello_graphics")