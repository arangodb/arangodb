target_sources(arangodbtests
  PRIVATE
  ThreadOwnedListTest.cpp
  ListOfNonOwnedListsTest.cpp)
target_link_libraries(arangodbtests
  arango_thread_owning_list)
