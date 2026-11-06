# The single-header form (vreg.h, generated) compiles warning-free from a
# consumer that includes Windows.h first, with the definitions it sets: the
# implementation as C, the declarations as C++ too. No /utf-8 either: the
# header's BOM has to say how to read it.
if(TARGET VReg_Detours)
    find_package(Detours REQUIRED)
endif()

function(vreg_amalgamation_check name)
    set(target VReg_AmalgamationCheck_${name})

    add_library(${target} OBJECT
        header.cpp
        impl.c
    )

    add_dependencies(${target}
        VReg_Amalgamation
    )

    # Deliberately not VReg::VReg: the consumer has no include/.
    target_include_directories(${target} PRIVATE
        ${VREG_AMALGAMATION_DIR}
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

    if(TARGET VReg_Detours)
        target_link_libraries(${target} PRIVATE
            Detours::Detours
        )

        target_compile_definitions(${target} PRIVATE
            VREG_WITH_DETOURS
        )
    endif()
endfunction()

# Every combination of WIN32_LEAN_AND_MEAN, UNICODE and NOMINMAX.
foreach(lean IN ITEMS 0 1)
    foreach(unicode IN ITEMS 0 1)
        foreach(nominmax IN ITEMS 0 1)
            vreg_amalgamation_check(${lean}${unicode}${nominmax}
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
    vreg_amalgamation_check(WinNT${winnt}
        _WIN32_WINNT=${winnt}
    )
endforeach()

# The whole suite against the generated header. The library comes from
# amalgamation_impl.cpp alone, and advapi32 from the header's pragma.
add_executable(VReg_AmalgamationTests
    ${VREG_TEST_SOURCES}
    amalgamation_impl.c
)

add_dependencies(VReg_AmalgamationTests
    VReg_Amalgamation
)

target_link_libraries(VReg_AmalgamationTests PRIVATE
    VReg_BuildSettings
    Catch2::Catch2WithMain
    Shlwapi
)

# The tests reach internal headers as well as the public ones.
target_include_directories(VReg_AmalgamationTests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${VREG_AMALGAMATION_DIR}
)

target_compile_definitions(VReg_AmalgamationTests PRIVATE
    VREG_TEST_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VREG_TEST_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VREG_TEST_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

catch_discover_tests(VReg_AmalgamationTests
    TEST_PREFIX "amalgamation: "
    PROPERTIES TIMEOUT 120
)
