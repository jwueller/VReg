
vreg_add_library(Core STATIC)
target_sources(
    VReg_Core
    PRIVATE
        ansi.cpp
        export_table.cpp
        handle_space.cpp
        memory_hive.cpp
        native_registry.cpp
        owned_values.cpp
        overlay_hive.cpp
        registry.cpp
        tree.cpp
        version.cpp
        virtual_registry.cpp
)
# NativeRegistry resolves from advapi32's export table and must find it
# loaded; the import makes the loader map it first.
target_link_libraries(VReg_Core PUBLIC advapi32)
# GetVersion() and GetVersionString() report the version computed on every
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
        -P ${PROJECT_SOURCE_DIR}/cmake/VRegVersion.cmake
    BYPRODUCTS ${vreg_version_header}
    VERBATIM
)
add_dependencies(VReg_Core VReg_Version)
target_include_directories(VReg_Core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
