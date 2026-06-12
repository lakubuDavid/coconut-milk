add_rules("plugin.compile_commands.autoupdate", {outputdir = ".", lsp = "clangd"})
add_rules("mode.debug", "mode.release")
add_requires("luajit 2.*", "sol2 ~3.3.*")
add_requires("nlohmann_json 3.12.0")
set_languages("c11", "c++23")
add_includedirs("thirdparty/webview/core/include")

-- =================================================================
-- Task: build TS->JS embed
-- =================================================================

task("coconut_bridge_embeds")
    set_menu {
        usage = "xmake coconut_bridge_embeds",
        description = "Build coconut bridge TS->JS + .d.ts and generate embed .h header"
    }
    set_category("build")
    on_run(function ()
        local ts_in   = "src/embeds/coconut.ts"
        local js_out  = "src/embeds/coconut.js"
        local dts_out = "src/embeds/coconut.d.ts"
        local hdr_out = "src/embeds/coconut_embed.h"

        os.run("bun build " .. ts_in .. " --outfile " .. js_out .. " --format esm")
        os.run("bunx tsc " .. ts_in .. " --declaration --emitDeclarationOnly --outDir src/embeds --lib ES2020,DOM --target ES2020")
        os.run("python3 scripts/js2c_to_header.py --input " .. js_out .. " --output " .. hdr_out .. " --symbol coconut_js_embed")
        cprint("[task] coconut-bridge-embeds: " .. js_out .. ", " .. dts_out .. ", " .. hdr_out)
    end)

-- =================================================================
-- Third-party library targets
-- =================================================================

target("webview")
    set_kind("static")
    add_includedirs("thirdparty/webview/core/include")
    add_frameworks("Cocoa", "WebKit", "Foundation")
    add_files("thirdparty/webview/core/src/webview.cc")
    set_languages("c11", "c++17")
    set_targetdir("$(buildir)/lib")

-- =================================================================
-- Core coconut binary (the one binary used by all examples)
-- =================================================================

target("coconut")
    set_kind("binary")
    add_includedirs("src", "thirdparty/webview/core/include")
    before_build(function ()
        if not os.isfile("src/embeds/coconut_embed.h") then
            os.run("xmake coconut_bridge_embeds")
        end
    end)
    add_frameworks("Cocoa", "WebKit", "Foundation")
    add_files("src/*.cpp")
    add_files("src/generators/*.cpp")
    add_files("src/platform/scheme_handler.cpp")
    if is_plat("macosx") then
        add_files("src/platform/darwin/*.cpp")
        add_files("src/platform/darwin/*.mm")
    elseif is_plat("windows") then
        add_files("src/platform/win/*.cpp")
    elseif is_plat("linux") then
        add_files("src/platform/linux/*.cpp")
    end
    add_packages("sol2", "luajit", "nlohmann_json")
    add_deps("webview")

-- =================================================================
-- Tests
-- =================================================================

target("coconut-milk-tests")
    set_kind("binary")
    add_includedirs("src", "tests")
    add_includedirs("thirdparty/webview/core/include")
    add_frameworks("Cocoa", "WebKit", "Foundation")
    add_deps("webview")
    add_files(
        "tests/*.cpp",
        "tests/**/*.cpp",
        "src/app.cpp",
        "src/bridge.cpp",
        "src/commands.cpp",
        "src/config.cpp",
        "src/context.cpp",
        "src/debug.cpp",
        "src/dialog.cpp",
        "src/error.cpp",
        "src/fs.cpp",
        "src/lifecycle.cpp",
        "src/lua_runtime.cpp",
        "src/window.cpp",
        "src/webview_transport.cpp",
        "src/platform/scheme_handler.cpp"
    )
    if is_plat("macosx") then
        add_files("src/platform/darwin/*.cpp")
        add_files("src/platform/darwin/*.mm")
    elseif is_plat("windows") then
        add_files("src/platform/win/*.cpp")
    elseif is_plat("linux") then
        add_files("src/platform/linux/*.cpp")
    end
    add_packages("sol2", "luajit", "nlohmann_json")
