-- =====================================================================
-- coconut-cli — generator + scaffolder front-end (no GUI, no runtime)
--
--   cd apps/coconut-cli && xmake build && ./build/*/release/coconut-cli
--
-- Fully self-contained: the generator and scaffolder sources are copied
-- into ./src (originally from apps/coconut/src) so this target builds
-- without referencing the coconut app tree.  The only dependency is
-- p-ranav/argparse (header-only) pulled via the xmake package manager.
-- =====================================================================

add_rules("plugin.compile_commands.autoupdate", {outputdir = ".", lsp = "clangd"})

add_rules("mode.debug", "mode.release")
set_languages("c++26")
add_requires("argparse v3.2")

target("coconut-cli")
    set_kind("binary")
    set_basename("coconut-cli")
    add_includedirs("src")
    add_packages("argparse")
    add_files("src/main.cpp")
    add_files("src/generators/main.cpp")
    add_files("src/new_project.cpp")
