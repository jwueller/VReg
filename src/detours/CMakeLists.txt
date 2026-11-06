find_package(Detours REQUIRED)

vreg_add_library(Detours STATIC)

target_sources(VReg_Detours PRIVATE
    hooks.c
)

# Consumers see the hooks' declarations in vreg.h through the same macro.
target_compile_definitions(VReg_Detours PUBLIC
    VREG_WITH_DETOURS
)

target_link_libraries(VReg_Detours PUBLIC
    VReg::VReg
    Detours::Detours
)
