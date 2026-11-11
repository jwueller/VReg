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

# The tests and the hooks compiled apart link the private functions.
target_compile_definitions(VReg_VReg PRIVATE
    VREG_INTERNAL
)

if(VREG_WITH_DETOURS)
    add_subdirectory(detours)
endif()
