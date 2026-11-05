# The single-header form (vreg.h, generated) compiles warning-free from a
# consumer that includes Windows.h first, whether or not it defines
# WIN32_LEAN_AND_MEAN, UNICODE and NOMINMAX, in every combination. No
# /utf-8 either: the header's BOM has to say how to read it.
if(TARGET VReg_Detours)
    find_package(Detours REQUIRED)
endif()

foreach(lean IN ITEMS 0 1)
    foreach(unicode IN ITEMS 0 1)
        foreach(nominmax IN ITEMS 0 1)
            set(_target VReg_AmalgamationCheck_${lean}${unicode}${nominmax})

            add_library(${_target} OBJECT
                impl.cpp
            )

            add_dependencies(${_target}
                VReg_Amalgamation
            )

            # Deliberately not VReg::Core: the consumer has no include/.
            target_include_directories(${_target} PRIVATE
                ${VREG_AMALGAMATION_DIR}
            )

            target_compile_features(${_target} PRIVATE
                cxx_std_20
            )

            target_compile_options(${_target} PRIVATE
                /W4
                /WX
                /permissive-
            )

            target_compile_definitions(${_target} PRIVATE
                $<${lean}:WIN32_LEAN_AND_MEAN>
                $<${unicode}:UNICODE>
                $<${unicode}:_UNICODE>
                $<${nominmax}:NOMINMAX>
            )

            if(TARGET VReg_Detours)
                target_link_libraries(${_target} PRIVATE
                    Detours::Detours
                )

                target_compile_definitions(${_target} PRIVATE
                    VREG_WITH_DETOURS
                )
            endif()
        endforeach()
    endforeach()
endforeach()

# The whole suite against the generated header. The library comes from
# amalgamation_impl.cpp alone, and advapi32 from the header's pragma.
add_executable(VReg_AmalgamationTests
    ${VREG_TEST_SOURCES}
    amalgamation_impl.cpp
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
