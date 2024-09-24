add_library(arango_async_registry_feature STATIC
  Feature.cpp
  RestHandler.cpp)
target_link_libraries(arango_async_registry_feature 
  PRIVATE
  arango_metrics)
