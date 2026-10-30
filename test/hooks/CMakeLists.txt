# Hooks are process-wide and never detach, so every test case runs in a process
# of its own (catch_discover_tests below).

set(_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)

foreach(_module A B)
    add_library(VReg_HookModule${_module} MODULE ../intercept_registry.h hook_module.cpp hook_mount.h)
    target_compile_definitions(VReg_HookModule${_module} PRIVATE VREG_HOOK_MODULE_NAME=L"${_module}")
    target_link_libraries(VReg_HookModule${_module} PRIVATE VReg::Core VReg::Detours VReg_BuildSettings)
    set_target_properties(VReg_HookModule${_module} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_dir})
endforeach()

add_executable(
    VReg_HookTests
    ../intercept_registry.h
    ../no_dialogs.cpp
    ../oracle.cpp
    ../oracle.h
    hook_mount.h
    test_hooks.cpp
)
target_link_libraries(VReg_HookTests PRIVATE VReg::Core VReg::Detours VReg_BuildSettings Catch2::Catch2WithMain ktmw32)
target_include_directories(VReg_HookTests PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(VReg_HookTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_dir})
add_dependencies(VReg_HookTests VReg_HookModuleA VReg_HookModuleB)
catch_discover_tests(VReg_HookTests PROPERTIES TIMEOUT 120)
