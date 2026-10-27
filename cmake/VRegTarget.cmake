# Settings every VReg library and test target builds with. They stay PRIVATE:
# a consumer's own macros (UNICODE or not) and warning levels are its business,
# so the public headers must compile under either.
add_library(VReg_BuildSettings INTERFACE)
target_compile_features(VReg_BuildSettings INTERFACE cxx_std_20)
target_compile_definitions(
    VReg_BuildSettings
    INTERFACE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
)
target_compile_options(VReg_BuildSettings INTERFACE /W4 /permissive- /utf-8)
if(PROJECT_IS_TOP_LEVEL)
    target_compile_options(VReg_BuildSettings INTERFACE /WX)
endif()

# Declares the library target `VReg_<name>`, exported as `VReg::<name>`.
function(vreg_add_library name)
    add_library(VReg_${name} ${ARGN})
    add_library(VReg::${name} ALIAS VReg_${name})
    set_target_properties(VReg_${name} PROPERTIES EXPORT_NAME ${name})
    target_compile_features(VReg_${name} PUBLIC cxx_std_20)
    target_include_directories(
        VReg_${name}
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(VReg_${name} PRIVATE $<BUILD_INTERFACE:VReg_BuildSettings>)
    set_property(GLOBAL APPEND PROPERTY VREG_LIBRARIES VReg_${name})
endfunction()
