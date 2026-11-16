# Automated tests (CTest). Labels: unit, golden, gpu (skips without a hardware renderer),
# timing (excluded by the presets), package

# --- SDL_test based suites -----------------------------------------------------------------------
set(SDL3GFX_TEST_LIB SDL3_gfx::SDL3_gfx)
if(NOT TARGET SDL3_gfx::SDL3_gfx)
    set(SDL3GFX_TEST_LIB SDL3_gfx::SDL3_gfx-static)
endif()

set(SDL3GFX_GOLDEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/golden/sdl3")
set(SDL3GFX_FAILED_DIR "${CMAKE_CURRENT_BINARY_DIR}/failed")
file(MAKE_DIRECTORY "${SDL3GFX_FAILED_DIR}")

add_executable(gfxtests
    testsuites.c
    testutils.c
    testutils.h
    test_primitives.c
    test_rotozoom.c
    test_imagefilter.c
    test_framerate.c
    test_gpu.c)
target_link_libraries(gfxtests PRIVATE ${SDL3GFX_TEST_LIB} SDL3::SDL3_test sdl3gfx_build_flags)
if(TARGET SDL3::SDL3)
    target_link_libraries(gfxtests PRIVATE SDL3::SDL3)
endif()
if(UNIX)
    target_link_libraries(gfxtests PRIVATE m)
endif()
target_compile_definitions(gfxtests PRIVATE
    GFXTEST_GOLDEN_DIR="${SDL3GFX_GOLDEN_DIR}"
    GFXTEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
set_target_properties(gfxtests PROPERTIES C_STANDARD 99 FOLDER test RUNTIME_OUTPUT_DIRECTORY "${SDL3GFX_RUNTIME_DIR}")
add_dependencies(gfxtests sdl3gfx_runtime)

foreach(suite Primitives Rotozoom ImageFilter Framerate FramerateTiming)
    add_test(NAME ${suite} COMMAND gfxtests --filter ${suite} --output-dir "${SDL3GFX_FAILED_DIR}"
        WORKING_DIRECTORY "${SDL3GFX_FAILED_DIR}")
    set_tests_properties(${suite} PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
        TIMEOUT 120)
endforeach()
set_tests_properties(Primitives Rotozoom PROPERTIES LABELS "golden;unit")
set_tests_properties(ImageFilter Framerate PROPERTIES LABELS unit)
set_tests_properties(FramerateTiming PROPERTIES LABELS timing RUN_SERIAL TRUE)

# Hardware renderer (real video driver, hidden window); the tests skip themselves without one.
add_test(NAME PrimitivesGPU COMMAND gfxtests --filter PrimitivesGPU --real-video --output-dir "${SDL3GFX_FAILED_DIR}"
    WORKING_DIRECTORY "${SDL3GFX_FAILED_DIR}")
set_tests_properties(PrimitivesGPU PROPERTIES LABELS gpu RUN_SERIAL TRUE TIMEOUT 120)

# Cross-version parity with an SDL2_gfx checkout (by default next to this one, if there is one):
# the same scenes must give the SDL2_gfx golden images, and the headers must declare the same API.
set(SDL3GFX_PARITY_SDL2GFX_DIR "${PROJECT_SOURCE_DIR}/../SDL2_gfx" CACHE PATH "SDL2_gfx checkout for the parity tests")
set(SDL3GFX_PARITY_GOLDEN_DIR "${SDL3GFX_PARITY_SDL2GFX_DIR}/test/golden/sdl2")
if(EXISTS "${SDL3GFX_PARITY_GOLDEN_DIR}")
    foreach(suite Primitives Rotozoom)
        add_test(NAME parity.${suite} COMMAND gfxtests --filter ${suite} --golden-dir "${SDL3GFX_PARITY_GOLDEN_DIR}"
            --output-dir "${SDL3GFX_FAILED_DIR}"
            WORKING_DIRECTORY "${SDL3GFX_FAILED_DIR}")
        set_tests_properties(parity.${suite} PROPERTIES LABELS parity TIMEOUT 120
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
    endforeach()
    # Same function prototypes and macros as the SDL2_gfx headers (decision D8)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME parity.api
            COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/scripts/check_api_parity.py"
                "${SDL3GFX_PARITY_SDL2GFX_DIR}" "${PROJECT_SOURCE_DIR}")
        set_tests_properties(parity.api PROPERTIES LABELS parity)
    endif()
endif()

# Regenerate the golden images and checksums from the current code:
#   cmake --build --preset win-debug --target gfxtests_generate_golden
add_custom_target(gfxtests_generate_golden
    COMMAND gfxtests --generate --golden-dir "${SDL3GFX_GOLDEN_DIR}"
    DEPENDS gfxtests
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Writing golden images to ${SDL3GFX_GOLDEN_DIR}"
    VERBATIM)

# --- Package test: install into a scratch prefix, then build and run a find_package() consumer.
if(SDL3GFX_INSTALL)
    add_test(NAME package.find_package
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=${PROJECT_BINARY_DIR}
            -DCONFIG=$<CONFIG>
            -DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/package-prefix
            -DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/consumer
            -DCONSUMER_BINARY=${CMAKE_CURRENT_BINARY_DIR}/consumer-build
            -DGENERATOR=${CMAKE_GENERATOR}
            -DC_COMPILER=${CMAKE_C_COMPILER}
            -DSDL3_DIR=${SDL3_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/consumer/run_consumer_test.cmake)
    set_tests_properties(package.find_package PROPERTIES LABELS package TIMEOUT 300)
endif()

if(SDL3GFX_LEGACY_COLOR_BYTE_ORDER)
    target_compile_definitions(gfxtests PRIVATE SDL3_GFX_LEGACY_COLOR_BYTE_ORDER)
endif()
