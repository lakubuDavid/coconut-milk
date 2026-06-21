add_rules("plugin.compile_commands.autoupdate", {outputdir = ".", lsp = "clangd"})
add_rules("mode.debug", "mode.release")
if is_mode("release") then
    add_requires("luajit 2.*", {configs = {shared = false, gc64 = true}})
    add_requires("sol2 ~3.3.*")
else
    add_requires("luajit 2.*", "sol2 ~3.3.*")
end
add_requires("nlohmann_json 3.12.0")
add_requires("lunasvg")
if is_plat("macosx") then
    set_languages("c23", "c++26")
else
    set_languages("c23", "c++23")
end
add_includedirs("thirdparty/webview/core/include")
add_includedirs("thirdparty")
add_defines("COCONUT_VERSION=\"0.1.0\"")

-- =================================================================
-- Task: build TS->JS embed
-- =================================================================

task("bridge_embeds")
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
    if is_plat("macosx") then
        add_frameworks("Cocoa", "WebKit", "Foundation")
    elseif is_plat("windows") then
        add_syslinks("user32", "gdi32", "ole32", "oleaut32", "shell32",
                     "shlwapi", "uuid", "comctl32", "advapi32", "version")
    elseif is_plat("linux") then
        add_cxflags("$(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.1)")
        add_ldflags("$(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.1)")
    end
    add_files("thirdparty/webview/core/src/webview.cc")
    add_defines("WEBVIEW_STATIC")
    set_languages("c11", "c++17")

-- =================================================================
-- Core coconut binary
-- =================================================================

target("coconut")
    set_kind("binary")
    add_includedirs("src", "thirdparty/webview/core/include")
    before_build(function ()
        if not os.isfile("src/embeds/coconut_embed.h") then
            os.run("xmake coconut_bridge_embeds")
        end
    end)
    if is_plat("macosx") then
        add_frameworks("Cocoa", "WebKit", "Foundation",
                       "AVFoundation", "UserNotifications",
                       "Contacts", "Photos", "Security",
                       "ApplicationServices", "ScreenCaptureKit")
    elseif is_plat("windows") then
        add_syslinks("user32", "gdi32", "ole32", "oleaut32", "shell32",
                     "shlwapi", "uuid", "comctl32", "advapi32", "version")
    elseif is_plat("linux") then
        add_cxflags("$(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.1 libnotify)")
        add_ldflags("$(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.1 libnotify)")
    end
    add_files("src/*.cpp")
    add_files("src/packages/*.cpp")
    add_files("src/generators/*.cpp")
    add_files("src/platform/scheme_handler.cpp")
    if is_plat("macosx") then
        add_files("src/platform/darwin/*.cpp")
        add_files("src/platform/darwin/*.mm")
        add_ldflags("-Wl,-sectcreate,__TEXT,__info_plist,$(projectdir)/res/Info.plist", {force = true})
    elseif is_plat("windows") then
        add_files("src/platform/win/*.cpp")
    elseif is_plat("linux") then
        add_files("src/platform/linux/*.cpp")
    end
    add_files("src/permissions.cpp")
    add_packages("sol2", "luajit", "nlohmann_json", "lunasvg")
    add_deps("webview")

-- =================================================================
-- Tests
-- =================================================================

target("tests")
    set_kind("binary")
    add_includedirs("src", "tests", "thirdparty/webview/core/include")
    add_includedirs("thirdparty")
    if is_plat("macosx") then
        add_frameworks("Cocoa", "WebKit", "Foundation",
                       "AVFoundation", "EventKit", "UserNotifications",
                       "CoreLocation", "Contacts", "Photos", "Security",
                       "ApplicationServices", "ScreenCaptureKit")
    elseif is_plat("windows") then
        add_syslinks("user32", "gdi32", "ole32", "oleaut32", "shell32",
                     "shlwapi", "uuid", "comctl32", "advapi32", "version",
                     "runtimeobject")
    elseif is_plat("linux") then
        add_cxflags("$(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.1 libnotify)")
        add_ldflags("$(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.1 libnotify)")
    end
    add_files("src/*.cpp")
    add_files("src/packages/*.cpp")
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
    remove_files("src/main.cpp")
    add_files("src/permissions.cpp")
    add_files("tests/*.cpp", "tests/**/*.cpp")
    add_packages("sol2", "luajit", "nlohmann_json", "lunasvg")
    add_deps("webview")
