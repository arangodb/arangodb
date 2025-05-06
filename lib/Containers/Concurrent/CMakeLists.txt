add_library(arango_thread_owning_list STATIC
  thread.cpp)
target_include_directories(arango_thread_owning_list PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arango_thread_owning_list
  PRIVATE
  arango_basic_utils
  arango_inspection)
