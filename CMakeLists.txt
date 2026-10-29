cmake_minimum_required(VERSION 3.25)

# Agent's choice: the public header owns the version and CMake reads it, so the
# header needs no generating and its siblings include it relatively. Passed
# over: configuring it into the build tree, which only angle-bracket includes
# through an extra include directory can reach.
file(STRINGS include/VReg/version.h vreg_version_lines REGEX "VREG_VERSION_(MAJOR|MINOR|PATCH) = [0-9]+;")
foreach(line IN LISTS vreg_version_lines)
    string(REGEX MATCH "VREG_VERSION_([A-Z]+) = ([0-9]+);" _ "${line}")
    set(vreg_version_${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
endforeach()

project(
    VReg
    VERSION ${vreg_version_MAJOR}.${vreg_version_MINOR}.${vreg_version_PATCH}
    DESCRIPTION "A virtualized Windows registry"
    HOMEPAGE_URL "https://github.com/jwueller/VReg"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "VReg targets Windows only.")
endif()

option(VREG_WITH_DETOURS "Build VReg::Detours (needs Microsoft Detours)" OFF)
if(VREG_WITH_DETOURS AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "ARM64")
    message(FATAL_ERROR "VReg::Detours does not support ARM64 yet (see \"Missing\" in README.md).")
endif()
option(VREG_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(cmake/VRegTarget.cmake)

add_subdirectory(src)

if(VREG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

include(cmake/VRegInstall.cmake)
