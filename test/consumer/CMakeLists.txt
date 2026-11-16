cmake_minimum_required(VERSION 3.21)
project(sdl3gfx_consumer LANGUAGES C)

find_package(SDL3_gfx REQUIRED CONFIG)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE SDL3_gfx::SDL3_gfx)
if(TARGET SDL3::SDL3)
    target_link_libraries(consumer PRIVATE SDL3::SDL3)
endif()
if(WIN32)
    add_custom_command(TARGET consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E
            "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:consumer>>,copy_if_different;$<TARGET_RUNTIME_DLLS:consumer>;$<TARGET_FILE_DIR:consumer>,true>"
        COMMAND_EXPAND_LISTS)
endif()
