vreg_add_library(VReg STATIC)

target_sources(VReg_VReg PUBLIC FILE_SET HEADERS BASE_DIRS ${PROJECT_SOURCE_DIR} FILES
    ${PROJECT_SOURCE_DIR}/vreg.h
)

target_sources(VReg_VReg PRIVATE
    vreg.c
)

# The native registry resolves from advapi32's export table and must find it
# loaded; the import makes the loader map it first.
target_link_libraries(VReg_VReg PUBLIC
    advapi32
)

# VRegGetFileVersion and VRegGetVersion report the version computed on every
# build (cmake/VRegVersion.cmake), so a commit made since configuring cannot
# leave them stale.
set(vreg_version_header ${CMAKE_CURRENT_BINARY_DIR}/generated/version_info.h)

add_custom_target(
    VReg_Version
    COMMAND
        ${CMAKE_COMMAND}
        -D SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -D OUTPUT=${vreg_version_header}
        -D COMMIT=${VREG_SOURCE_COMMIT}
        -D STRICT=${PROJECT_IS_TOP_LEVEL}
        -P ${PROJECT_SOURCE_DIR}/cmake/VRegVersion.cmake
    BYPRODUCTS ${vreg_version_header}
    VERBATIM
)

add_dependencies(VReg_VReg
    VReg_Version
)

target_include_directories(VReg_VReg PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

if(VREG_WITH_DETOURS)
    add_subdirectory(detours)
endif()
