add_executable(arangodbtests_wcc
  WeaklyConnectedComponentsTest.cpp)

target_include_directories(arangodbtests_wcc PRIVATE
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_link_libraries(arangodbtests_wcc velocypack_utils fmt gtest)

# add_subdirectory(Actor)
