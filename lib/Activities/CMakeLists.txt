add_library(arango_activities_interface INTERFACE)
target_include_directories(arango_activities_interface INTERFACE
  ${PROJECT_SOURCE_DIR}/lib
  include)


add_library(arango_activities STATIC)
target_include_directories(arango_activities PUBLIC
  include)
target_link_libraries(arango_activities
PUBLIC
  arango_thread_owning_list
  arango_shared
  arango_inspection)

add_library(arango_activities_global STATIC)
target_link_libraries(arango_activities_global
PUBLIC
  arango_activities)

add_subdirectory(src)
