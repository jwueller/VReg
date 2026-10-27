configure_file(version.h.in ${PROJECT_BINARY_DIR}/include/VReg/version.h @ONLY)

vreg_add_library(Core STATIC)
target_sources(
    VReg_Core
    PRIVATE
        handle_space.cpp
        version.cpp
)
target_include_directories(VReg_Core PUBLIC $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)
