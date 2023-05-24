add_library(arango_pregel_with_actors STATIC
  AggregatorHandler.cpp
  Utils.cpp)
target_include_directories(arango_pregel_with_actors
  PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod)
target_link_libraries(arango_pregel_with_actors
  PRIVATE
  arango_inspection
  arango_actor)
target_link_libraries(arango_pregel_with_actors
  PUBLIC
  absl::flat_hash_map)
add_library(arango_pregel STATIC
  AlgoRegistry.cpp
  ExecutionNumber.cpp
  IncomingCache.cpp
  IndexHelpers.cpp
  OutgoingCache.cpp
  PregelFeature.cpp
  PregelMetrics.cpp)
target_link_libraries(arango_pregel
  arango
  arango_agency
  arango_pregel_with_actors
  boost_boost
  arango_actor
  boost_system)
target_include_directories(arango_pregel PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_subdirectory(Actor)
add_subdirectory(Algos)
add_subdirectory(Conductor)
add_subdirectory(GraphStore)
add_subdirectory(REST)
add_subdirectory(Worker)
add_subdirectory(SystemCollection)
