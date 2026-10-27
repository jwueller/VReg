find_package(Detours REQUIRED)

vreg_add_library(Detours STATIC)
target_sources(VReg_Detours PRIVATE hooks.cpp)
target_link_libraries(VReg_Detours PUBLIC VReg::Core Detours::Detours)
# The hooks resolve their targets the way NativeRegistry does.
target_include_directories(VReg_Detours PRIVATE ${PROJECT_SOURCE_DIR}/src)
