add_library(arango_async_registry_server STATIC
  Feature.cpp
  RestHandler.cpp)
target_link_libraries(arango_async_registry_server 
  PRIVATE
  arango_metrics)
