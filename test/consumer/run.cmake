# Script mode (-P): install, configure, build and run the consumer project.

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed (${result}): ${ARGN}")
    endif()
endfunction()

set(prefix ${CONSUMER_WORK_DIR}/prefix)
set(build ${CONSUMER_WORK_DIR}/build)
file(REMOVE_RECURSE ${CONSUMER_WORK_DIR})

run(${CMAKE_COMMAND} --install ${VREG_BINARY_DIR} --config ${VREG_CONFIG} --prefix ${prefix})
run(
    ${CMAKE_COMMAND}
    -S ${CONSUMER_SOURCE_DIR}
    -B ${build}
    -G ${CMAKE_GENERATOR}
    -D CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -D CMAKE_CXX_COMPILER_TARGET=${CMAKE_CXX_COMPILER_TARGET}
    -D CMAKE_BUILD_TYPE=${VREG_CONFIG}
    -D CMAKE_CONFIGURATION_TYPES=${VREG_CONFIG}
    -D CMAKE_PREFIX_PATH=${prefix}
    -D Detours_ROOT=${DETOURS_PREFIX}
    -D CONSUMER_WITH_DETOURS=${CONSUMER_WITH_DETOURS}
)
run(${CMAKE_COMMAND} --build ${build} --config ${VREG_CONFIG})

file(GLOB_RECURSE consumer ${build}/consumer.exe)
run(${consumer})
