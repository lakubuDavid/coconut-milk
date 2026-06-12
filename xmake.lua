-- Add the autoupdate rule globally or inside a target
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".", lsp = "clangd"})
add_rules("mode.debug", "mode.release")
add_requires("luajit 2.*", "sol2 ~3.3.*")
add_requires("nlohmann_json 3.12.0")

add_includedirs("thirdparty/webview/core/include")

set_languages("c11", "c++23")

-- Build Coconut bridge frontend bundle (TS -> JS + .d.ts) and embed header.
task("coconut_bridge_embeds")
    set_menu {
        usage = "xmake coconut_bridge_embeds",
        description = "Build coconut bridge TS->JS + .d.ts and generate embed .h header"
    }
    set_category("build")
    on_run(function ()
        local ts_in = "src/embeds/coconut.ts"
        local js_out = "src/embeds/coconut.js"
        local dts_out = "src/embeds/coconut.d.ts"
        local header_out = "src/embeds/coconut_embed.h"

        -- 1) JS bundle (uses var — that's fine, it's embedded artifact)
        os.run("bun build " .. ts_in .. " --outfile " .. js_out .. " --format esm")

        -- 2) Declarations (Bun doesn't emit .d.ts here)
        os.run("bunx tsc " .. ts_in .. " --declaration --emitDeclarationOnly --outDir src/embeds --lib ES2020,DOM --target ES2020")

        -- 3) C header embed (byte array)
        os.run("python3 scripts/js2c_to_header.py --input " .. js_out .. " --output " .. header_out .. " --symbol coconut_js_embed")

        cprint("[task] coconut-bridge-embeds generated: " .. js_out .. ", " .. dts_out .. ", " .. header_out)
    end)


target("webview")
    set_kind("static")
    add_includedirs("thirdparty/webview/core/include")
    add_frameworks("Cocoa", "WebKit", "Foundation")
    add_files("thirdparty/webview/core/src/webview.cc")
    set_languages("c11", "c++17")
    set_targetdir("$(buildir)/lib")

target("coconut")
    set_kind("binary")
    set_rundir("$(projectdir)")
    before_build(function (target)
        os.run("xmake coconut_bridge_embeds")
    end)
    add_includedirs("src")
    add_includedirs("thirdparty/webview/core/include")
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
    add_packages("sol2")
    add_packages("luajit")
    add_packages("nlohmann_json")
    add_deps("webview")

target("calculator-vue")
    set_kind("binary")
    set_basename("coconut")
    set_rundir("$(projectdir)/examples/calculator-vue")
    before_build(function (target)
        os.run("xmake coconut_bridge_embeds")
    end)
    add_includedirs("src")
    add_includedirs("thirdparty/webview/core/include")
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
    add_packages("sol2")
    add_packages("luajit")
    add_packages("nlohmann_json")
    add_deps("webview")


target("ocr-app")
    set_kind("binary")
    set_basename("coconut")
    set_rundir("$(projectdir)/examples/ocr-app")
    before_build(function (target)
        os.run("xmake coconut_bridge_embeds")
    end)
    add_includedirs("src")
    add_includedirs("thirdparty/webview/core/include")
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
    add_packages("sol2")
    add_packages("luajit")
    add_packages("nlohmann_json")
    add_deps("webview")

target("lua-html-app")
    set_kind("binary")
    set_basename("coconut")
    set_rundir("$(projectdir)/examples/lua-html-app")
    before_build(function (target)
        os.run("xmake coconut_bridge_embeds")
    end)
    add_includedirs("src")
    add_includedirs("thirdparty/webview/core/include")
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
    add_packages("sol2")
    add_packages("luajit")
    add_packages("nlohmann_json")
    add_deps("webview")

-- Generator sources are compiled as part of the main `coconut` target.
-- No standalone generator binary.

-- Test target
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
    add_packages("sol2")
    add_packages("luajit")
    add_packages("nlohmann_json")

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

