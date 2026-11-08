# vreg.h owns the version: VREG_VERSION_MAJOR, _MINOR, _PATCH and _TWEAK.
# Sets outVar in the caller's scope to MAJOR.MINOR.PATCH.TWEAK as the header
# at path defines them.
function(vreg_read_version path outVar)
    file(STRINGS "${path}" defines REGEX "^#define VREG_VERSION_(MAJOR|MINOR|PATCH|TWEAK) ")
    set(parts)
    foreach(part IN ITEMS MAJOR MINOR PATCH TWEAK)
        if(NOT defines MATCHES "#define VREG_VERSION_${part} ([0-9]+)")
            message(FATAL_ERROR "vreg.h does not define VREG_VERSION_${part} as a number.")
        endif()
        list(APPEND parts "${CMAKE_MATCH_1}")
    endforeach()
    list(JOIN parts "." version)
    set(${outVar} "${version}" PARENT_SCOPE)
endfunction()
