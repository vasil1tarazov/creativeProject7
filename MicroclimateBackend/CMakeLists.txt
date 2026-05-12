cmake_minimum_required(VERSION 3.16)
project(MicroclimateBackend VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MicroclimateBackend main.cpp)

target_include_directories(MicroclimateBackend PRIVATE .)

# Для Windows нужно линковать библиотеку сокетов
if(WIN32)
    target_link_libraries(MicroclimateBackend PRIVATE ws2_32)
endif()