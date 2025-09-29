find_package(Catch2 3 REQUIRED)

add_executable(test
  test_package_merge.cpp
  test_lzss.cpp
  test_ring_buffer.cpp
)

target_include_directories(test
  PRIVATE
  $<TARGET_PROPERTY:gzip::gzip,INCLUDE_DIRECTORIES>
)

target_link_libraries(test
  PRIVATE
  gzip::gzip
  Catch2::Catch2WithMain
)

