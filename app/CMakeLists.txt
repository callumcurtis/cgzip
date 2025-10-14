add_executable(cgzip
  main.cpp
)

target_include_directories(cgzip
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cgzip
    PRIVATE
    cgzip::cgzip
)

install(TARGETS cgzip DESTINATION bin)
