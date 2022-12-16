add_executable(arangodbtests_wcc
  WeaklyConnectedComponentsTest.cpp)
target_include_directories(arangodbtests_wcc PRIVATE
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/arangod/Pregel)
target_link_libraries(arangodbtests_wcc velocypack_utils fmt gtest_main)

add_executable(pregel_rest_options RestOptionsTest.cpp)
target_include_directories(pregel_rest_options PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod)
target_link_libraries(pregel_rest_options velocypack_utils fmt arango gtest_main)

add_subdirectory(Actor)
