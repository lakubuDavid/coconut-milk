-- =====================================================================
-- coconut-cli — generator + scaffolder front-end (no GUI, no runtime)
--
--   cd apps/coconut-cli && xmake build && ./build/*/release/coconut-cli
--
-- Reuses the generator and scaffold sources from the coconut app via
-- include paths (single source of truth); this target adds only a thin
-- argv front-end.  No external packages required.
-- =====================================================================

add_rules("mode.debug", "mode.release")
set_languages("c++26")

target("coconut-cli")
    set_kind("binary")
    set_basename("coconut-cli")
    add_includedirs("../coconut/src")
    add_files("src/main.cpp")
    -- Generator + scaffolder live in the coconut app tree (shared source).
    add_files("../coconut/src/generators/main.cpp")
    add_files("../coconut/src/new_project.cpp")
