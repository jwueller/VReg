find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(
    VReg_Tests
    intercept_registry.h
    no_dialogs.cpp
    oracle.cpp
    oracle.h
    regfile_test.h
    scenario.h
    test_concurrency.cpp
    test_handle.cpp
    test_handle_probe.cpp
    test_native_registry.cpp
    test_notify.cpp
    test_oracle.cpp
    test_oracle_enum.cpp
    test_oracle_get.cpp
    test_oracle_values.cpp
    test_overlay_hive.cpp
    test_regfile_apply.cpp
    test_regfile_delta.cpp
    test_regfile_export.cpp
    test_regfile_parse.cpp
    test_regfile_roundtrip.cpp
    test_registry.cpp
    test_size_only.cpp
    test_status.cpp
    test_version.cpp
    test_virtual_registry.cpp
    thread.h
    win32_api.h
)
target_link_libraries(VReg_Tests PRIVATE VReg::RegFile VReg::Core VReg_BuildSettings Catch2::Catch2WithMain Shlwapi)
# The tests reach internal headers as well as the public ones.
target_include_directories(VReg_Tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
# What GetFileVersion() must report.
target_compile_definitions(
    VReg_Tests
    PRIVATE
        VREG_TEST_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        VREG_TEST_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        VREG_TEST_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)# A hung test fails instead of stalling an unattended run.
catch_discover_tests(VReg_Tests PROPERTIES TIMEOUT 120)

# Installs this build into a scratch prefix and builds `consumer/` against it
# with find_package, the way a package manager would.
add_test(
    NAME consumer_find_package
    COMMAND
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
