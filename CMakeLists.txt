cmake_minimum_required(VERSION 3.25)

project(
    VReg
    VERSION 0.1.0
    DESCRIPTION "A virtualized Windows registry"
    HOMEPAGE_URL "https://github.com/jwueller/VReg"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "VReg targets Windows only.")
endif()

option(VREG_WITH_DETOURS "Build VReg::Detours (needs Microsoft Detours)" OFF)
option(VREG_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(cmake/VRegTarget.cmake)

add_subdirectory(src)

if(VREG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

include(cmake/VRegInstall.cmake)
