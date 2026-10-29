cmake_minimum_required(VERSION 3.25)

project(
    VReg
    DESCRIPTION "A virtualized Windows registry"
    HOMEPAGE_URL "https://github.com/jwueller/VReg"
    LANGUAGES CXX
)

# vcpkg.json owns the version; cmake/VRegVersion.cmake derives the rest.
# PROJECT_VERSION_MAJOR to _TWEAK are numeric (TWEAK counts the commits), and
# PROJECT_VERSION is the full semver string (0.3.0-preview.5+<commit>).
set(VREG_SOURCE_COMMIT "" CACHE STRING "The commit to report for a build outside a git checkout")
if(NOT VREG_SOURCE_COMMIT MATCHES "^([0-9A-Za-z-]+([.][0-9A-Za-z-]+)*)?$")
    message(FATAL_ERROR "VREG_SOURCE_COMMIT is not semver build metadata: ${VREG_SOURCE_COMMIT}")
endif()
include(cmake/VRegVersion.cmake)
vreg_compute_version("${PROJECT_SOURCE_DIR}" "${VREG_SOURCE_COMMIT}" "${PROJECT_IS_TOP_LEVEL}")
foreach(part IN ITEMS MAJOR MINOR PATCH TWEAK)
    set(PROJECT_VERSION_${part} "${VREG_VERSION_${part}}")
    set(VReg_VERSION_${part} "${VREG_VERSION_${part}}")
endforeach()
set(PROJECT_VERSION "${VREG_VERSION}")
set(VReg_VERSION "${VREG_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/vcpkg.json)

if(NOT WIN32)
    message(FATAL_ERROR "VReg targets Windows only.")
endif()

option(VREG_WITH_DETOURS "Build VReg::Detours (needs Microsoft Detours)" OFF)
option(VREG_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(VREG_INSTALL "Install VReg along with the project that builds it" ${PROJECT_IS_TOP_LEVEL})

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
