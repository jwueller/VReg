cmake_minimum_required(VERSION 3.25)
project(VRegConsumer LANGUAGES CXX)

option(CONSUMER_WITH_DETOURS "Also link VReg::Detours" OFF)

if(CONSUMER_WITH_DETOURS)
    find_package(VReg CONFIG REQUIRED COMPONENTS Core RegFile Detours)
else()
    find_package(VReg CONFIG REQUIRED COMPONENTS Core RegFile)
endif()

add_executable(consumer
    main.cpp
)

target_link_libraries(consumer PRIVATE
    VReg::Core
    VReg::RegFile
)

# The version find_package found, which the linked copy must report.
target_compile_definitions(consumer PRIVATE
    CONSUMER_VERSION_MAJOR=${VReg_VERSION_MAJOR}
    CONSUMER_VERSION_MINOR=${VReg_VERSION_MINOR}
    CONSUMER_VERSION_PATCH=${VReg_VERSION_PATCH}
)

if(CONSUMER_WITH_DETOURS)
    target_link_libraries(consumer PRIVATE
        VReg::Detours
    )

    target_compile_definitions(consumer PRIVATE
        CONSUMER_WITH_DETOURS
    )
endif()
