
vreg_add_library(Core STATIC)

target_sources(VReg_Core PUBLIC FILE_SET HEADERS BASE_DIRS ${PROJECT_SOURCE_DIR}/include FILES
    ${PROJECT_SOURCE_DIR}/include/VReg/core.h
    ${PROJECT_SOURCE_DIR}/include/VReg/handle.h
    ${PROJECT_SOURCE_DIR}/include/VReg/hive.h
    ${PROJECT_SOURCE_DIR}/include/VReg/memory_hive.h
    ${PROJECT_SOURCE_DIR}/include/VReg/native_registry.h
    ${PROJECT_SOURCE_DIR}/include/VReg/overlay_hive.h
    ${PROJECT_SOURCE_DIR}/include/VReg/owned_values.h
    ${PROJECT_SOURCE_DIR}/include/VReg/registry.h
    ${PROJECT_SOURCE_DIR}/include/VReg/version.h
    ${PROJECT_SOURCE_DIR}/include/VReg/virtual_registry.h
)

target_sources(VReg_Core PRIVATE
    ansi.cpp
    ansi.h
    export_table.cpp
    export_table.h
    handle_space.cpp
    handle_space.h
    lock.h
    memory_hive.cpp
    native_registry.cpp
    overlay_hive.cpp
    owned_values.cpp
    registry.cpp
    retry.h
    status.h
    tree.cpp
    tree.h
    value_type.h
    version.cpp
    virtual_registry.cpp
)

# NativeRegistry resolves from advapi32's export table and must find it
# loaded; the import makes the loader map it first.
target_link_libraries(VReg_Core PUBLIC
    advapi32
)

# GetFileVersion() and GetVersion() report the version computed on every
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

add_dependencies(VReg_Core
    VReg_Version
)

target_include_directories(VReg_Core PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
