cmake_minimum_required(VERSION 3.25)

include(cmake/VRegVersion.cmake)
vreg_read_version(${CMAKE_CURRENT_SOURCE_DIR}/vreg.h vregVersion)

project(
    VReg
    VERSION ${vregVersion}
    DESCRIPTION "A virtualized Windows registry"
    HOMEPAGE_URL "https://github.com/jwueller/VReg"
    LANGUAGES C CXX
)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/vreg.h)

if(NOT WIN32)
    message(FATAL_ERROR "VReg targets Windows only.")
endif()

option(VREG_WITH_DETOURS "Build VReg::Detours (needs Microsoft Detours)" OFF)
option(VREG_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(VREG_INSTALL "Install VReg along with the project that builds it" ${PROJECT_IS_TOP_LEVEL})
option(VREG_ANALYZE "Run MSVC code analysis (/analyze) over the library sources" OFF)

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(cmake/VRegTarget.cmake)

add_subdirectory(src)

if(VREG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(VREG_INSTALL)
    include(cmake/VRegInstall.cmake)
endif()
