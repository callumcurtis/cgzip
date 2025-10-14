add_library(cgzipLib
  gz.cpp
  deflate.cpp
)

target_include_directories(cgzipLib
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(cgzip::cgzip ALIAS cgzipLib)
