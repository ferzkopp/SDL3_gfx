# Interactive demo programs (formerly Tests/). They use SDL3's SDL_test_common for window and
# renderer command-line options; run with --help for usage.

set(SDL3GFX_DEMO_LIB SDL3_gfx::SDL3_gfx)
if(NOT TARGET SDL3_gfx::SDL3_gfx)
    set(SDL3GFX_DEMO_LIB SDL3_gfx::SDL3_gfx-static)
endif()

function(sdl3gfx_add_demo name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE ${SDL3GFX_DEMO_LIB} sdl3gfx_build_flags ${ARGN})
    if(TARGET SDL3::SDL3)
        target_link_libraries(${name} PRIVATE SDL3::SDL3)
    endif()
    if(UNIX)
        target_link_libraries(${name} PRIVATE m)
    endif()
    set_target_properties(${name} PROPERTIES
        C_STANDARD 99
        FOLDER demo
        RUNTIME_OUTPUT_DIRECTORY "${SDL3GFX_RUNTIME_DIR}")
    # Demos load the sample images from their working directory (the executable folder).
    add_dependencies(${name} sdl3gfx_runtime)
endfunction()

sdl3gfx_add_demo(testgfx SDL3::SDL3_test)
sdl3gfx_add_demo(testrotozoom SDL3::SDL3_test)
sdl3gfx_add_demo(testframerate SDL3::SDL3_test)
sdl3gfx_add_demo(testimagefilter)
