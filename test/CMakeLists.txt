find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# Shared with test/amalgamation, so the files are absolute.
set(VREG_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_hive_registry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/intercept_registry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/no_dialogs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/objects.h
    ${CMAKE_CURRENT_SOURCE_DIR}/oracle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/oracle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/owned_values.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/owned_values.h
    ${CMAKE_CURRENT_SOURCE_DIR}/regfile_test.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scenario.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_concurrency.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_handle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_handle_probe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_native_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_notify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_oracle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_oracle_enum.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_oracle_get.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_oracle_values.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_overlay_hive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_regfile_apply.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_regfile_delta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_regfile_export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_regfile_parse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_regfile_roundtrip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_size_only.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_version.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_virtual_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/win32_api.h
)

add_executable(VReg_Tests
    ${VREG_TEST_SOURCES}
)

target_link_libraries(VReg_Tests PRIVATE
    VReg::VReg
    VReg_BuildSettings
    Catch2::Catch2WithMain
    Shlwapi
)

# The tests reach internal headers as well as the public ones.
target_include_directories(VReg_Tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

# What GetFileVersion() must report.
target_compile_definitions(VReg_Tests PRIVATE
    VREG_TEST_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VREG_TEST_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VREG_TEST_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# A hung test fails instead of stalling an unattended run.
catch_discover_tests(VReg_Tests PROPERTIES TIMEOUT 120)

# Installs this build into a scratch prefix and builds `consumer/` against it
# with find_package, the way a package manager would.
add_test(NAME consumer_find_package COMMAND
    ${CMAKE_COMMAND}
    -D VREG_BINARY_DIR=${PROJECT_BINARY_DIR}
    -D VREG_CONFIG=$<CONFIG>
    -D CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/consumer
    -D CONSUMER_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/$<CONFIG>
    -D CMAKE_GENERATOR=${CMAKE_GENERATOR}
    -D CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -D CMAKE_CXX_COMPILER_TARGET=${CMAKE_CXX_COMPILER_TARGET}
    -D CONSUMER_WITH_DETOURS=$<IF:$<TARGET_EXISTS:VReg_Detours>,ON,OFF>
    -D DETOURS_PREFIX=$<$<TARGET_EXISTS:VReg_Detours>:${Detours_INCLUDE_DIR}/..>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/consumer/run.cmake
)

if(TARGET VReg_Detours)
    add_subdirectory(hooks)
endif()

add_subdirectory(headers)
add_subdirectory(amalgamation)
