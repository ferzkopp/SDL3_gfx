# Fuzz targets (fuzz_<name>.c, libFuzzer entry point LLVMFuzzerTestOneInput).
#
# - Always: <target>_replay executables that run the committed corpus (fuzz/corpus/<target>/) as
#   CTest tests (label fuzz-regression).
# - With SDL3GFX_FUZZ=ON (presets win-fuzz / linux-fuzz): <target> libFuzzer executables built
#   with AddressSanitizer (+ UBSan on clang). Run e.g.
#       build\win-fuzz\bin\fuzz_primitives.exe -max_total_time=600 fuzz\corpus\primitives
#   New interesting inputs are added to the corpus directory; crashes are written as crash-<hash>.

set(SDL3GFX_FUZZ_TARGETS primitives rotozoom imagefilter font)

set(SDL3GFX_FUZZ_LIB SDL3_gfx::SDL3_gfx)
if(NOT TARGET SDL3_gfx::SDL3_gfx)
    set(SDL3GFX_FUZZ_LIB SDL3_gfx::SDL3_gfx-static)
endif()

foreach(name ${SDL3GFX_FUZZ_TARGETS})
    set(corpus_dir "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}")

    add_executable(fuzz_${name}_replay fuzz_${name}.c replay_main.c fuzz_common.h)
    target_link_libraries(fuzz_${name}_replay PRIVATE ${SDL3GFX_FUZZ_LIB})
    if(UNIX)
        target_link_libraries(fuzz_${name}_replay PRIVATE m)
    endif()
    set_target_properties(fuzz_${name}_replay PROPERTIES C_STANDARD 99 FOLDER fuzz RUNTIME_OUTPUT_DIRECTORY "${SDL3GFX_RUNTIME_DIR}")
    add_dependencies(fuzz_${name}_replay sdl3gfx_runtime)

    if(SDL3GFX_TESTS)
        file(GLOB corpus CONFIGURE_DEPENDS "${corpus_dir}/*")
        if(corpus)
            add_test(NAME fuzz.${name} COMMAND fuzz_${name}_replay ${corpus})
            set_tests_properties(fuzz.${name} PROPERTIES LABELS fuzz-regression TIMEOUT 300
                ENVIRONMENT "SDL_VIDEODRIVER=dummy")
        endif()
    endif()

    if(SDL3GFX_FUZZ)
        add_executable(fuzz_${name} fuzz_${name}.c fuzz_common.h)
        target_link_libraries(fuzz_${name} PRIVATE ${SDL3GFX_FUZZ_LIB})
        if(MSVC)
            target_compile_options(fuzz_${name} PRIVATE /fsanitize=fuzzer)
        else()
            target_compile_options(fuzz_${name} PRIVATE -fsanitize=fuzzer)
            target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer)
            target_link_libraries(fuzz_${name} PRIVATE m)
        endif()
        set_target_properties(fuzz_${name} PROPERTIES C_STANDARD 99 FOLDER fuzz RUNTIME_OUTPUT_DIRECTORY "${SDL3GFX_RUNTIME_DIR}")
        add_dependencies(fuzz_${name} sdl3gfx_runtime)
    endif()
endforeach()
