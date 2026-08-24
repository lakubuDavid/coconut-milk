add_rules("plugin.compile_commands.autoupdate", {outputdir = ".", lsp = "clangd"})
add_rules("mode.debug", "mode.release")

-- Sanitizer modes:  xmake f -m asan   /   xmake f -m tsan   /   xmake f -m ubsan
-- These are defined as rules that only activate when is_mode() matches.
-- They must be declared BEFORE targets that consume them.
add_rules("mode.asan", "mode.tsan", "mode.ubsan")

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
add_defines("COCONUT_VERSION=\"0.1.1\"")

-- =================================================================
-- Task: build TS->JS embed
-- =================================================================

task("bridge_embeds")
    set_menu {
        usage = "xmake bridge_embeds",
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
        os.run("python3 ../../scripts/js2c_to_header.py --input " .. js_out .. " --output " .. hdr_out .. " --symbol coconut_js_embed")
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
    add_includedirs("src","src/core", "thirdparty/webview/core/include")
    before_build(function ()
        if not os.isfile("src/embeds/coconut_embed.h") then
            os.run("xmake bridge_embeds")
        end
    end)
    if is_plat("macosx") then
        add_frameworks("Cocoa", "WebKit", "Foundation",
                       "AVFoundation", "UserNotifications",
                       "Contacts", "Photos", "Security",
                       "ApplicationServices", "ScreenCaptureKit")
        -- C++26 makes enum-arithmetic a hard error (not suppressible).
        -- .mm files use C++23 where it's only a deprecation warning.
        add_mxxflags("-std=c++23")
    elseif is_plat("windows") then
        add_syslinks("user32", "gdi32", "ole32", "oleaut32", "shell32",
                     "shlwapi", "uuid", "comctl32", "advapi32", "version")
    elseif is_plat("linux") then
        add_cxflags("$(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.1 libnotify)")
        add_ldflags("$(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.1 libnotify)")
    end
    add_files("src/*.cpp")
    add_files("src/core/*.cpp")
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
    if is_plat("macosx") then
        add_files("src/platform/darwin/runloop.cpp")
    else
        -- Non-mac platforms fall back to the polling stub until a native
        -- GMainLoop / Win32-message integration lands.
        add_files("src/platform/stub/runloop.cpp")
    end
    add_files("src/modules/*.cpp")
    add_packages("sol2", "luajit", "nlohmann_json", "lunasvg")
    add_deps("webview")

-- =================================================================
-- Tests
-- =================================================================

target("tests")
    set_kind("binary")
    if is_plat("macosx") then
        -- Pin to Apple's toolchain: PATH-first LLVM clang mis-compiles
        -- .mm sources here (drops the -std flag for ObjC++).
        set_toolchains("xcode")
    end
    add_includedirs("src", "src/core", "tests", "thirdparty/webview/core/include")
    add_includedirs("thirdparty")
    if is_plat("macosx") then
        add_frameworks("Cocoa", "WebKit", "Foundation",
                       "AVFoundation", "EventKit", "UserNotifications",
                       "CoreLocation", "Contacts", "Photos", "Security",
                       "ApplicationServices", "ScreenCaptureKit")
        -- C++26 makes enum-arithmetic a hard error; .mm files use C++23
        add_mxxflags("-std=c++23")
    elseif is_plat("windows") then
        add_syslinks("user32", "gdi32", "ole32", "oleaut32", "shell32",
                     "shlwapi", "uuid", "comctl32", "advapi32", "version",
                     "runtimeobject")
    elseif is_plat("linux") then
        add_cxflags("$(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.1 libnotify)")
        add_ldflags("$(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.1 libnotify)")
    end
    add_files("src/*.cpp")
    add_files("src/core/*.cpp")
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
    add_files("src/modules/*.cpp")
    add_files("tests/*.cpp", "tests/**/*.cpp")
    -- Module-test mains have their own binaries (test_modules_*); keep them
    -- out of the aggregate test binary to avoid duplicate main()/symbols.
    remove_files("tests/modules/**/*.cpp")
    add_packages("sol2", "luajit", "nlohmann_json", "lunasvg")
    add_deps("webview")

-- =================================================================
-- Module tests — self-contained binaries that compile only the
-- sources needed for a single subsystem.  Define NO_PLATFORM so
-- stub platform .cpp files supply the linker symbols instead of
-- the real macOS/Win/Linux platform implementations.
-- =================================================================

target("test_modules_workers")
    set_kind("binary")
    add_defines("NO_PLATFORM")
    add_includedirs("src", "src/core", "tests")
    add_includedirs("thirdparty/webview/core/include")
    add_includedirs("thirdparty")
    add_files("src/core/worker.cpp")
    add_files("src/core/exec_command.cpp")
    add_files("src/modules/*.cpp")
    add_files("src/config.cpp")
    add_files("src/commands.cpp")
    add_files("src/context.cpp")
    add_files("src/debug.cpp")
    add_files("src/error.cpp")
    add_files("src/fs.cpp")
    add_files("src/dispatch.cpp")
    add_files("src/bridge.cpp")
    add_files("src/main_runtime.cpp")
    add_files("src/window.cpp")
    add_files("src/dialog.cpp")
    add_files("src/view_events.cpp")
    -- Stub platform satisfies all platform symbols:
    add_files("src/platform/stub/*.cpp")
    -- Module test binary:
    add_files("tests/modules/workers/main.cpp")
    add_files("src/packages/env.cpp")
    add_files("src/store.cpp")
    add_files("src/hotreload.cpp")
    add_files("src/packages/notify.cpp")
    add_files("src/packages/clipboard.cpp")
    add_files("src/generators/*.cpp")
    add_files("src/webview_transport.cpp")
    add_packages("sol2", "luajit", "nlohmann_json")
    if is_plat("macosx") then
        add_frameworks("CoreFoundation")
    end

target("test_modules_multi_workers")
    set_kind("binary")
    add_defines("NO_PLATFORM")
    add_includedirs("src", "src/core", "tests")
    add_includedirs("thirdparty/webview/core/include")
    add_includedirs("thirdparty")
    add_files("src/core/worker.cpp")
    add_files("src/core/exec_command.cpp")
    add_files("src/modules/*.cpp")
    add_files("src/config.cpp")
    add_files("src/commands.cpp")
    add_files("src/context.cpp")
    add_files("src/debug.cpp")
    add_files("src/error.cpp")
    add_files("src/fs.cpp")
    add_files("src/dispatch.cpp")
    add_files("src/bridge.cpp")
    add_files("src/main_runtime.cpp")
    add_files("src/window.cpp")
    add_files("src/dialog.cpp")
    add_files("src/view_events.cpp")
    add_files("src/platform/stub/*.cpp")
    add_files("src/packages/env.cpp")
    add_files("src/store.cpp")
    add_files("src/hotreload.cpp")
    add_files("src/packages/notify.cpp")
    add_files("src/packages/clipboard.cpp")
    add_files("src/generators/*.cpp")
    add_files("src/webview_transport.cpp")
    add_files("tests/modules/multi_workers/main.cpp")
    add_packages("sol2", "luajit", "nlohmann_json")
    if is_plat("macosx") then
        add_frameworks("CoreFoundation")
    end

-- ── Sanitizer rules (defined at bottom, after add_requires) ──────
--   xmake f -m asan   → AddressSanitizer
--   xmake f -m tsan   → ThreadSanitizer
--   xmake f -m ubsan  → UndefinedBehaviorSanitizer
--
-- Each rule checks is_mode() so only the active mode's policy fires.
rule("mode.asan")
    after_load(function (target)
        if is_mode("asan") then
            target:set("policy", "build.sanitizer.address", true)
            target:set("symbols", "debug")
        end
    end)
rule("mode.tsan")
    after_load(function (target)
        if is_mode("tsan") then
            target:set("policy", "build.sanitizer.thread", true)
            target:set("symbols", "debug")
        end
    end)
rule("mode.ubsan")
    after_load(function (target)
        if is_mode("ubsan") then
            target:set("policy", "build.sanitizer.undefined", true)
            target:set("symbols", "debug")
        end
    end)
