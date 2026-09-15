cmake_minimum_required(VERSION 3.15)

project(async-cpp-logger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(
    logger
    Logger.cpp
)

target_link_libraries(logger PRIVATE Threads::Threads)
