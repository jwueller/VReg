vreg_add_library(VReg STATIC)

target_sources(VReg_VReg PUBLIC FILE_SET HEADERS BASE_DIRS ${PROJECT_SOURCE_DIR}/include FILES
    ${PROJECT_SOURCE_DIR}/include/VReg/core.h
    ${PROJECT_SOURCE_DIR}/include/VReg/handle.h
    ${PROJECT_SOURCE_DIR}/include/VReg/hive.h
    ${PROJECT_SOURCE_DIR}/include/VReg/native_registry.h
    ${PROJECT_SOURCE_DIR}/include/VReg/owned_values.h
    ${PROJECT_SOURCE_DIR}/include/VReg/regfile.h
    ${PROJECT_SOURCE_DIR}/include/VReg/registry.h
    ${PROJECT_SOURCE_DIR}/include/VReg/version.h
    ${PROJECT_SOURCE_DIR}/include/VReg/virtual_registry.h
)

target_sources(VReg_VReg PRIVATE
    core/ansi.cpp
    core/ansi.h
    core/export_table.cpp
    core/export_table.h
    core/handle.c
    core/handle_space.c
    core/handle_space.h
    core/lock.h
    core/hive.cpp
    core/native_registry.cpp
    core/node.h
    core/owned_values.cpp
    core/registry.cpp
    core/retry.h
    core/rrf.h
    core/status.h
    core/value_type.h
    core/version.cpp
    core/virtual_registry.cpp
    regfile/apply.cpp
    regfile/export.cpp
    regfile/parse.cpp
    regfile/text.cpp
    regfile/text.h
    regfile/writer.cpp
    regfile/writer.h
)

# NativeRegistry resolves from advapi32's export table and must find it
# loaded; the import makes the loader map it first.
target_link_libraries(VReg_VReg PUBLIC
    advapi32
)

# GetFileVersion() and GetVersion() report the version computed on every
# build (cmake/VRegVersion.cmake), so a commit made since configuring cannot
# leave them stale.
set(vreg_version_header ${CMAKE_CURRENT_BINARY_DIR}/generated/core/version_info.h)

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
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

if(VREG_WITH_DETOURS)
    add_subdirectory(detours)
endif()
