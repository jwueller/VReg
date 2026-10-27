cmake_minimum_required(VERSION 3.25)
project(VRegConsumer LANGUAGES CXX)

option(CONSUMER_WITH_DETOURS "Also link VReg::Detours" OFF)

if(CONSUMER_WITH_DETOURS)
    find_package(VReg CONFIG REQUIRED COMPONENTS Core RegFile Detours)
else()
    find_package(VReg CONFIG REQUIRED COMPONENTS Core RegFile)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE VReg::Core VReg::RegFile)
if(CONSUMER_WITH_DETOURS)
    target_link_libraries(consumer PRIVATE VReg::Detours)
    target_compile_definitions(consumer PRIVATE CONSUMER_WITH_DETOURS)
endif()
