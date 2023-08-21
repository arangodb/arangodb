add_library(arango_tests_build_id OBJECT
  BuildIdTest.cpp)

target_link_libraries(arango_tests_build_id
  PUBLIC
    gtest
    arango_build_id)

add_executable(arangodbtests_build_id
  EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_build_id
  gtest_main
  arango_build_id
  arango_tests_build_id)
target_link_options(arangodbtests_build_id
  PUBLIC
  -Wl,--build-id)
