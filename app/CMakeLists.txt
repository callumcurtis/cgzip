add_executable(gzip
  uvgz.cpp
)

target_include_directories(gzip
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gzip
    PRIVATE
    gzip::gzip
)

install(TARGETS gzip DESTINATION bin)
