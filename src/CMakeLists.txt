add_library(gzipLib
  foo.cpp
)

target_include_directories(gzipLib
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(gzip::gzip ALIAS gzipLib)

