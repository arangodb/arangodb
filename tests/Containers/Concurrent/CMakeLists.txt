target_sources(arangodbtests
  PRIVATE
  ThreadOwnedListTest.cpp
  ListOfListsTest.cpp)
target_link_libraries(arangodbtests
  arango_thread_owning_list)
