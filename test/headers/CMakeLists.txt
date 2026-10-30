# The public headers compile warning-free whether or not the consumer defines
# WIN32_LEAN_AND_MEAN, UNICODE and NOMINMAX, in every combination.
foreach(lean IN ITEMS 0 1)
    foreach(unicode IN ITEMS 0 1)
        foreach(nominmax IN ITEMS 0 1)
            set(_target VReg_HeaderCheck_${lean}${unicode}${nominmax})

            add_library(${_target} OBJECT
                header_check.cpp
            )

            # Only for the include directories and the language standard.
            target_link_libraries(${_target} PRIVATE
                VReg::Core
                VReg::RegFile
            )

            target_compile_options(${_target} PRIVATE
                /W4
                /WX
                /permissive-
                /utf-8
            )

            target_compile_definitions(${_target} PRIVATE
                $<${lean}:WIN32_LEAN_AND_MEAN>
                $<${unicode}:UNICODE>
                $<${unicode}:_UNICODE>
                $<${nominmax}:NOMINMAX>
            )
        endforeach()
    endforeach()
endforeach()
