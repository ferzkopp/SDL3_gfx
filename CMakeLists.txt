cmake_minimum_required(VERSION 3.21)

# Single source of truth for the version: the macros in SDL3_gfxPrimitives.h.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/SDL3_gfxPrimitives.h" _sdl3gfx_version_lines
     REGEX "^#define[ \t]+SDL3_GFXPRIMITIVES_(MAJOR|MINOR|MICRO)[ \t]+[0-9]+")
foreach(_part MAJOR MINOR MICRO)
    string(REGEX REPLACE ".*SDL3_GFXPRIMITIVES_${_part}[ \t]+([0-9]+).*" "\\1" _sdl3gfx_${_part} "${_sdl3gfx_version_lines}")
endforeach()

project(SDL3_gfx
    VERSION ${_sdl3gfx_MAJOR}.${_sdl3gfx_MINOR}.${_sdl3gfx_MICRO}
    DESCRIPTION "Graphics primitives, rotozoomer, framerate control and image filters for SDL3"
    HOMEPAGE_URL "https://github.com/ferzkopp/SDL3_gfx"
    LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CheckLibraryExists)

set(SDL3GFX_SDL_REQUIRED_VERSION 3.2.0)

# Emscripten: static library only; tests, demos and the benchmark need a desktop SDL3.
set(_sdl3gfx_desktop ${PROJECT_IS_TOP_LEVEL})
set(_sdl3gfx_shared_default ON)
if(EMSCRIPTEN)
    set(_sdl3gfx_desktop OFF)
    set(_sdl3gfx_shared_default OFF)
endif()

option(SDL3GFX_SHARED  "Build the shared SDL3_gfx library" ${_sdl3gfx_shared_default})
option(SDL3GFX_STATIC  "Build the static SDL3_gfx library" OFF)
option(SDL3GFX_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(SDL3GFX_DEMOS   "Build the interactive demo programs" ${_sdl3gfx_desktop})
option(SDL3GFX_TESTS   "Build the automated tests (CTest)" ${_sdl3gfx_desktop})
option(SDL3GFX_WERROR  "Treat compiler warnings as errors" OFF)
option(SDL3GFX_ANALYZE "Run the MSVC code analyzer (/analyze) on the library sources" OFF)
option(SDL3GFX_FUZZ    "Build libFuzzer targets; instruments everything with AddressSanitizer" OFF)
option(SDL3GFX_BENCH   "Build the benchmark program" ${_sdl3gfx_desktop})
option(SDL3GFX_DOCS    "Add the 'docs' target (Doxygen HTML) if Doxygen is found" ${PROJECT_IS_TOP_LEVEL})
option(SDL3GFX_LEGACY_COLOR_BYTE_ORDER
    "...Color functions decode the color in memory byte order (SDL2_gfx up to 1.0.4, SDL3_gfx 1.0.x) instead of 0xRRGGBBAA" OFF)

if(SDL3GFX_FUZZ)
    # Instrument the library and all executables (tests then also run under ASan).
    if(MSVC)
        add_compile_options(/fsanitize=address)
        add_link_options(/INCREMENTAL:NO)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

if(EMSCRIPTEN AND SDL3GFX_SHARED)
    message(FATAL_ERROR "Emscripten builds support only the static library (-DSDL3GFX_SHARED=OFF)")
endif()
if(EMSCRIPTEN)
    set(SDL3GFX_STATIC ON)
endif()

if(NOT SDL3GFX_SHARED AND NOT SDL3GFX_STATIC)
    message(FATAL_ERROR "Enable at least one of SDL3GFX_SHARED or SDL3GFX_STATIC")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

if(PROJECT_IS_TOP_LEVEL)
    # Executables and DLLs side by side so demos/tests run without PATH changes.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()
set(SDL3GFX_RUNTIME_DIR "${PROJECT_BINARY_DIR}/bin")

if(EMSCRIPTEN)
    find_package(SDL3 ${SDL3GFX_SDL_REQUIRED_VERSION} CONFIG QUIET)
    if(NOT SDL3_FOUND)
        # No SDL3 build in CMAKE_PREFIX_PATH: use the SDL3 port of Emscripten.
        add_library(sdl3gfx_emscripten_sdl3 INTERFACE)
        target_compile_options(sdl3gfx_emscripten_sdl3 INTERFACE "--use-port=sdl3")
        target_link_options(sdl3gfx_emscripten_sdl3 INTERFACE "--use-port=sdl3")
        add_library(SDL3::SDL3 ALIAS sdl3gfx_emscripten_sdl3)
    endif()
else()
    find_package(SDL3 ${SDL3GFX_SDL_REQUIRED_VERSION} CONFIG REQUIRED)
endif()
# SDL3::SDL3 is an interface target for SDL3::SDL3-shared or SDL3::SDL3-static
set(SDL3GFX_SDL_TARGET SDL3::SDL3)
if(TARGET SDL3::SDL3-shared)
    set(_sdl3gfx_sdl_file_target SDL3::SDL3-shared)
endif()

# Files that demos and tests need next to their executables: the SDL3 DLL (Windows) and the sample
# images. Copied by a single target to avoid parallel copies of the same files.
file(GLOB SDL3GFX_TEST_DATA CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/test/data/*.bmp")
set(_sdl3gfx_runtime_files ${SDL3GFX_TEST_DATA})
if(WIN32 AND _sdl3gfx_sdl_file_target)
    list(APPEND _sdl3gfx_runtime_files "$<TARGET_FILE:${_sdl3gfx_sdl_file_target}>")
endif()
if(MSVC AND SDL3GFX_FUZZ)
    # MSVC AddressSanitizer runtime, so instrumented programs also start outside a VS dev shell.
    get_filename_component(_sdl3gfx_cl_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    file(GLOB _sdl3gfx_asan_dll "${_sdl3gfx_cl_dir}/clang_rt.asan_dynamic-x86_64.dll")
    list(APPEND _sdl3gfx_runtime_files ${_sdl3gfx_asan_dll})
endif()
add_custom_target(sdl3gfx_runtime
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SDL3GFX_RUNTIME_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sdl3gfx_runtime_files} "${SDL3GFX_RUNTIME_DIR}"
    COMMENT "Copying SDL3 runtime and sample images to ${SDL3GFX_RUNTIME_DIR}"
    VERBATIM)

set(SDL3GFX_PUBLIC_HEADERS
    SDL3_framerate.h
    SDL3_gfxPrimitives.h
    SDL3_imageFilter.h
    SDL3_rotozoom.h)
set(SDL3GFX_SOURCES
    SDL3_framerate.c
    SDL3_gfxPrimitives.c
    SDL3_gfxPrimitives_font.h
    SDL3_imageFilter.c
    SDL3_rotozoom.c)

# Warning and hardening flags shared by the library, tests and demos.
add_library(sdl3gfx_build_flags INTERFACE)
if(MSVC)
    target_compile_options(sdl3gfx_build_flags INTERFACE /W4 /sdl /guard:cf $<$<BOOL:${SDL3GFX_WERROR}>:/WX>)
    target_link_options(sdl3gfx_build_flags INTERFACE /guard:cf)
    target_compile_definitions(sdl3gfx_build_flags INTERFACE _CRT_SECURE_NO_WARNINGS)
elseif(EMSCRIPTEN)
    target_compile_options(sdl3gfx_build_flags INTERFACE
        -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes
        $<$<BOOL:${SDL3GFX_WERROR}>:-Werror>)
else()
    target_compile_options(sdl3gfx_build_flags INTERFACE
        -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes
        -fstack-protector-strong
        $<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2>
        $<$<BOOL:${SDL3GFX_WERROR}>:-Werror>)
endif()

set(SDL3GFX_TARGETS)

function(sdl3gfx_configure_library target)
    target_sources(${target} PRIVATE ${SDL3GFX_SOURCES} ${SDL3GFX_PUBLIC_HEADERS})
    check_library_exists(m sin "" HAVE_LIBM)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SDL3_gfx>)
    target_link_libraries(${target}
        PUBLIC ${SDL3GFX_SDL_TARGET}
        PRIVATE $<BUILD_INTERFACE:sdl3gfx_build_flags>)
    if(HAVE_LIBM)
        target_link_libraries(${target} PRIVATE m)
    endif()
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    if(SDL3GFX_LEGACY_COLOR_BYTE_ORDER)
        target_compile_definitions(${target} PRIVATE SDL3_GFX_LEGACY_COLOR_BYTE_ORDER)
    endif()
    if(MSVC AND SDL3GFX_ANALYZE)
        target_compile_options(${target} PRIVATE /analyze /analyze:external-)
    endif()
endfunction()

if(SDL3GFX_SHARED)
    add_library(SDL3_gfx-shared SHARED)
    sdl3gfx_configure_library(SDL3_gfx-shared)
    target_compile_definitions(SDL3_gfx-shared PRIVATE DLL_EXPORT INTERFACE LIBSDL3_GFX_DLL_IMPORT)
    # Only functions declared in the public headers are exported (BLD-001).
    # SONAME libSDL3_gfx.so.<major>: 2.x is not ABI compatible with the 1.0.x float API of the SDL3 port.
    set_target_properties(SDL3_gfx-shared PROPERTIES
        OUTPUT_NAME SDL3_gfx
        EXPORT_NAME SDL3_gfx-shared
        RUNTIME_OUTPUT_DIRECTORY "${SDL3GFX_RUNTIME_DIR}"
        C_VISIBILITY_PRESET hidden
        SOVERSION ${PROJECT_VERSION_MAJOR}
        VERSION ${PROJECT_VERSION})
    add_library(SDL3_gfx::SDL3_gfx-shared ALIAS SDL3_gfx-shared)
    list(APPEND SDL3GFX_TARGETS SDL3_gfx-shared)
endif()

if(SDL3GFX_STATIC)
    add_library(SDL3_gfx-static STATIC)
    sdl3gfx_configure_library(SDL3_gfx-static)
    # Avoid a name clash with the DLL import library on Windows.
    set_target_properties(SDL3_gfx-static PROPERTIES
        OUTPUT_NAME $<IF:$<BOOL:${MSVC}>,SDL3_gfx-static,SDL3_gfx>
        EXPORT_NAME SDL3_gfx-static
        POSITION_INDEPENDENT_CODE ON)
    add_library(SDL3_gfx::SDL3_gfx-static ALIAS SDL3_gfx-static)
    list(APPEND SDL3GFX_TARGETS SDL3_gfx-static)
endif()

# SDL3_gfx::SDL3_gfx: the shared library if built, otherwise the static one (like SDL3 satellite libraries)
if(SDL3GFX_SHARED)
    add_library(SDL3_gfx::SDL3_gfx ALIAS SDL3_gfx-shared)
else()
    add_library(SDL3_gfx::SDL3_gfx ALIAS SDL3_gfx-static)
endif()

if(SDL3GFX_DEMOS)
    add_subdirectory(demo)
endif()

if(SDL3GFX_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(SDL3GFX_TESTS OR SDL3GFX_FUZZ)
    add_subdirectory(fuzz)
endif()

if(SDL3GFX_BENCH)
    add_subdirectory(bench)
endif()

# API documentation: cmake --build --preset <preset> --target docs -> <build>/docs/html (not installed).
# Doxygen warnings (e.g. undocumented public symbols) fail the target.
if(SDL3GFX_DOCS)
    find_package(Doxygen OPTIONAL_COMPONENTS dot)
    if(DOXYGEN_FOUND)
        set(SDL3GFX_DOCS_OUTPUT "${PROJECT_BINARY_DIR}/docs")
        if(TARGET Doxygen::dot)
            set(SDL3GFX_DOCS_HAVE_DOT YES)
            get_filename_component(SDL3GFX_DOCS_DOT_PATH "${DOXYGEN_DOT_EXECUTABLE}" DIRECTORY)
        else()
            set(SDL3GFX_DOCS_HAVE_DOT NO)
            set(SDL3GFX_DOCS_DOT_PATH "")
        endif()
        configure_file(docs/Doxyfile.in "${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)
        add_custom_target(docs
            COMMAND Doxygen::doxygen "${PROJECT_BINARY_DIR}/Doxyfile"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Generating API documentation in ${SDL3GFX_DOCS_OUTPUT}/html"
            VERBATIM)
    else()
        message(STATUS "Doxygen not found: no 'docs' target")
    endif()
endif()
if(SDL3GFX_INSTALL)
    set(SDL3GFX_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/SDL3_gfx")

    if(TARGET sdl3gfx_emscripten_sdl3)
        list(APPEND SDL3GFX_TARGETS sdl3gfx_emscripten_sdl3)
    endif()
    install(TARGETS ${SDL3GFX_TARGETS} EXPORT SDL3_gfxTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES ${SDL3GFX_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/SDL3_gfx)
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/SDL3_gfx)
    if(MSVC AND SDL3GFX_SHARED)
        install(FILES $<TARGET_PDB_FILE:SDL3_gfx-shared> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
    endif()

    install(EXPORT SDL3_gfxTargets NAMESPACE SDL3_gfx:: DESTINATION ${SDL3GFX_CMAKE_DIR})
    configure_package_config_file(cmake/SDL3_gfxConfig.cmake.in
        "${PROJECT_BINARY_DIR}/SDL3_gfxConfig.cmake"
        INSTALL_DESTINATION ${SDL3GFX_CMAKE_DIR})
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/SDL3_gfxConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion)
    install(FILES
        "${PROJECT_BINARY_DIR}/SDL3_gfxConfig.cmake"
        "${PROJECT_BINARY_DIR}/SDL3_gfxConfigVersion.cmake"
        DESTINATION ${SDL3GFX_CMAKE_DIR})


    set(SDL3GFX_PC_LIB SDL3_gfx)
    if(NOT SDL3GFX_SHARED AND MSVC)
        set(SDL3GFX_PC_LIB SDL3_gfx-static)
    endif()
    set(SDL3GFX_PC_LIBS_PRIVATE "")
    if(HAVE_LIBM)
        set(SDL3GFX_PC_LIBS_PRIVATE " -lm")
    endif()
    configure_file(cmake/sdl3-gfx.pc.in "${PROJECT_BINARY_DIR}/sdl3-gfx.pc" @ONLY)
    install(FILES "${PROJECT_BINARY_DIR}/sdl3-gfx.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
