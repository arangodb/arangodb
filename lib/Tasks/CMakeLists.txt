add_library(arango_task_registry STATIC
  task_registry.cpp)
target_include_directories(arango_task_registry PUBLIC
  include)
target_link_libraries(arango_task_registry PUBLIC arango fmt)
