target_sources(arangodbtests
  PRIVATE
  ThreadOwnedListTest.cpp
  ListOfNonOwnedListsTest.cpp)
target_link_libraries(arangodbtests
  arango_thread_owning_list)

add_library(arango_test_shared OBJECT
  SharedTest.cpp)
target_link_libraries(arango_test_shared PRIVATE
  arango_shared
  arango_inspection
  velocypack
  gtest)
add_executable(arangodbtests_shared EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_shared
  arango_test_shared
  gtest_main)
