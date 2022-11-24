add_executable(arangodbtests_zkd EXCLUDE_FROM_ALL
  Conversion.cpp
  Library.cpp
  main.cpp)

target_link_libraries(arangodbtests_zkd
  arango_rocksdb
  gtest)

target_include_directories(arangodbtests_zkd PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod)

add_test(NAME zkd
         COMMAND arangodbtests_zkd)
