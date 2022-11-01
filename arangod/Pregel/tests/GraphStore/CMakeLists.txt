add_executable(arangodbtests_pregel_graphstore main.cpp)

target_include_directories(arangodbtests_pregel_graphstore
  PRIVATE
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_BINARY_DIR}/lib
    ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_link_libraries(arangodbtests_pregel_graphstore
  velocypack
  fmt
  gtest)
