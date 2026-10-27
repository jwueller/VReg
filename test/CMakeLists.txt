find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(
    VReg_Tests
    test_handle.cpp
    test_handle_probe.cpp
    test_version.cpp
)
target_link_libraries(VReg_Tests PRIVATE VReg::Core VReg_BuildSettings Catch2::Catch2WithMain)
# The tests reach internal headers as well as the public ones.
target_include_directories(VReg_Tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
catch_discover_tests(VReg_Tests)

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
        -P ${CMAKE_CURRENT_SOURCE_DIR}/consumer/run.cmake
)
