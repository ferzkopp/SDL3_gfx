# Installs the current build into PREFIX, then configures, builds and runs the consumer project.
# Invoked by CTest (see test/CMakeLists.txt) with -D variables.

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        list(JOIN ARGN " " cmd)
        message(FATAL_ERROR "Command failed (${rc}): ${cmd}")
    endif()
endfunction()

file(REMOVE_RECURSE "${PREFIX}" "${CONSUMER_BINARY}")

run(${CMAKE_COMMAND} --install "${BUILD_DIR}" --prefix "${PREFIX}" --config "${CONFIG}")

if(NOT EXISTS "${PREFIX}/lib/pkgconfig/sdl3-gfx.pc")
    message(FATAL_ERROR "pkg-config file was not installed")
endif()

run(${CMAKE_COMMAND} -S "${CONSUMER_SOURCE}" -B "${CONSUMER_BINARY}" -G "${GENERATOR}"
    "-DCMAKE_C_COMPILER=${C_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DCMAKE_PREFIX_PATH=${PREFIX}"
    "-DSDL3_DIR=${SDL3_DIR}")
run(${CMAKE_COMMAND} --build "${CONSUMER_BINARY}" --config "${CONFIG}")

file(GLOB_RECURSE consumer_exe "${CONSUMER_BINARY}/consumer" "${CONSUMER_BINARY}/consumer.exe")
if(NOT consumer_exe)
    message(FATAL_ERROR "consumer executable not found")
endif()
list(GET consumer_exe 0 consumer_exe)
run("${consumer_exe}")
