target_sources(arangoserver PRIVATE
  Feature.cpp
  Metrics.cpp
  RestHandler.cpp)
target_link_libraries(arangoserver
  arango_forest)
