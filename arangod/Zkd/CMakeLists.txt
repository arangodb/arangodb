add_library(arango_zkd STATIC
  ZkdHelper.cpp)

target_link_libraries(arango_zkd arango)
