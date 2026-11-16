# Benchmark program (see docs/PERFORMANCE.md). Run from a Release build, e.g.
#   build\win-release\bin\gfxbench.exe --csv bench.csv
#   build\win-release\bin\gfxbench.exe --renderer direct3d11
set(SDL3GFX_BENCH_LIB SDL3_gfx::SDL3_gfx)
if(NOT TARGET SDL3_gfx::SDL3_gfx)
    set(SDL3GFX_BENCH_LIB SDL3_gfx::SDL3_gfx-static)
endif()

add_executable(gfxbench gfxbench.c)
target_link_libraries(gfxbench PRIVATE ${SDL3GFX_BENCH_LIB} sdl3gfx_build_flags)
if(TARGET SDL3::SDL3)
    target_link_libraries(gfxbench PRIVATE SDL3::SDL3)
endif()
if(UNIX)
    target_link_libraries(gfxbench PRIVATE m)
endif()
set_target_properties(gfxbench PROPERTIES C_STANDARD 99 FOLDER bench RUNTIME_OUTPUT_DIRECTORY "${SDL3GFX_RUNTIME_DIR}")
add_dependencies(gfxbench sdl3gfx_runtime)
