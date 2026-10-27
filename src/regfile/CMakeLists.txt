vreg_add_library(RegFile STATIC)
target_sources(
    VReg_RegFile
    PRIVATE
        apply.cpp
        export.cpp
        parse.cpp
        text.cpp
        writer.cpp
)
target_link_libraries(VReg_RegFile PUBLIC VReg::Core)
