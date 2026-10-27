cmake_minimum_required(VERSION 3.25)
project(VRegConsumer LANGUAGES CXX)

find_package(VReg CONFIG REQUIRED COMPONENTS Core)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE VReg::Core)
