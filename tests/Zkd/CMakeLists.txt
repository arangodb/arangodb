add_executable(arangodbtests_zkd
  Conversion.cpp
  Library.cpp
  main.cpp)

target_link_libraries(arangodbtests_zkd
  arango_rocksdb
  gtest)

target_include_directories(arangodbtests_zkd PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod)

