target_sources(arango_pregel PRIVATE
  Conductor.cpp)

add_library(arango_pregel_conductor STATIC)
target_include_directories(arango_pregel_conductor
  PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod)

target_link_libraries(arango_pregel_conductor
  PRIVATE
  arango_inspection
  arango_actor)

target_link_libraries(arango_pregel
  arango_pregel_conductor)

add_subdirectory(ExecutionStates)
