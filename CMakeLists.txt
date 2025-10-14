cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(cgzip LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
add_subdirectory(app)
add_subdirectory(test)
