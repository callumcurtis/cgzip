cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(gzip LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if (ENABLE_COVERAGE)
  # Even with -O0, artifacts appear in the coverage.
  # Adding -fno-elide-constructors and -fno-default-inline resolves the issue:
  # https://github.com/linux-test-project/lcov/issues/129#issuecomment-1374604567
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g --coverage -fno-elide-constructors -fno-default-inline")
endif()

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(app)
