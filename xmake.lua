-- set project name
set_project("vulkaninja")

-- set project version
set_version("1.0.0")

-- set language version: C++ 20
set_languages("cxx20")

-- global options
option("enable_extension") -- enable extensions?
    set_default(true)
option_end()

option("build_examples") -- build examples?
    set_default(true)
option_end()

-- if build on windows
if is_plat("windows") then
    if is_mode("debug") then
        set_runtimes("MDd")
        add_links("ucrtd")
    else
        set_runtimes("MD")
    end
end

-- enable exceptions
set_exceptions("cxx")

-- global rules
rule("copy_assets")
    after_build(function (target)
        local asset_files = target:values("asset_files")
        if asset_files then
            for _, pattern in ipairs(asset_files) do
                local files = os.files(pattern)
                for _, file in ipairs(files) do
                    local relpath = path.relative(file, os.projectdir())
                    local target_dir = path.join(target:targetdir(), path.directory(relpath))
                    os.mkdir(target_dir)
                    os.cp(file, target_dir)
                    print("Copying asset: " .. file .. " -> " .. target_dir)
                end
            end
        end
    end)
rule_end()

rule("imguiconfig")
    set_extensions(".ini")

    on_build_file(function (target, sourcefile, opt) end)

    after_build_file(function (target, sourcefile, opt)
        if path.basename(sourcefile) ~= "imgui" then
            return
        end
        local output_path = path.join(target:targetdir(), path.filename(sourcefile))
        os.cp(sourcefile, output_path)
        print("Copying imgui config: " .. sourcefile .. " -> " .. output_path)
    end)
rule_end()

add_rules("mode.debug", "mode.release")
add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

-- add my own xmake-repo here
add_repositories("my-xmake-repo https://github.com/zzxzzk115/xmake-repo.git dev")

includes("vulkaninja")

if has_config("build_examples") then
    includes("examples")
end