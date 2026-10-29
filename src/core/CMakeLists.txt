
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
