configure_file(version.h.in ${PROJECT_BINARY_DIR}/include/VReg/version.h @ONLY)

vreg_add_library(Core STATIC)
target_sources(
    VReg_Core
    PRIVATE
        ansi.cpp
        export_table.cpp
        handle_space.cpp
        memory_hive.cpp
        native_registry.cpp
        overlay_hive.cpp
        registry.cpp
        tree.cpp
        version.cpp
        virtual_registry.cpp
)
target_include_directories(VReg_Core PUBLIC $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)
# NativeRegistry resolves from advapi32's export table and must find it
# loaded; the import makes the loader map it first.
target_link_libraries(VReg_Core PUBLIC advapi32)
