# vreg.h compiles warning-free from a consumer that includes Windows.h first,
# with the definitions it sets: the implementation as C, the declarations as
# C++ too. No /utf-8: the header's BOM has to say how to read it.
if(TARGET VReg_Detours)
    find_package(Detours REQUIRED)
endif()

function(vreg_header_check name)
    set(target VReg_HeaderCheck_${name})

    add_library(${target} OBJECT
        header_check.c
        header_check.cpp
    )

    # Deliberately not VReg::VReg: a consumer of the single header has
    # nothing but the file.
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}
    )

    target_compile_features(${target} PRIVATE
        c_std_99
        cxx_std_20
    )

    target_compile_options(${target} PRIVATE
        /W4
        /WX
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
    )

    target_compile_definitions(${target} PRIVATE
        ${ARGN}
    )

    # The hooks' declarations need nothing; their implementation needs
    # Detours.
    if(TARGET VReg_Detours)
        target_link_libraries(${target} PRIVATE
            Detours::Detours
        )

        target_compile_definitions(${target} PRIVATE
            VREG_WITH_DETOURS
        )
    else()
        target_compile_definitions(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:VREG_WITH_DETOURS>
        )
    endif()
endfunction()

# Every combination of WIN32_LEAN_AND_MEAN, UNICODE and NOMINMAX.
foreach(lean IN ITEMS 0 1)
    foreach(unicode IN ITEMS 0 1)
        foreach(nominmax IN ITEMS 0 1)
            vreg_header_check(${lean}${unicode}${nominmax}
                $<${lean}:WIN32_LEAN_AND_MEAN>
                $<${unicode}:UNICODE>
                $<${unicode}:_UNICODE>
                $<${nominmax}:NOMINMAX>
            )
        endforeach()
    endforeach()
endforeach()

# The oldest target VReg supports, and Windows 7, a common one for retrofits.
foreach(winnt IN ITEMS 0x0600 0x0601)
    vreg_header_check(WinNT${winnt}
        _WIN32_WINNT=${winnt}
    )
endforeach()
