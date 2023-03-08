add_library(arango_pregel STATIC
  AggregatorHandler.cpp
  AlgoRegistry.cpp
  ExecutionNumber.cpp
  IncomingCache.cpp
  IndexHelpers.cpp
  OutgoingCache.cpp
  PregelFeature.cpp
  PregelMetrics.cpp
  Utils.cpp)

target_link_libraries(arango_pregel
  arango
  arango_agency
  boost_boost
  arango_actor
  boost_system)

target_include_directories(arango_pregel PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_subdirectory(Actor)
add_subdirectory(Algos)
add_subdirectory(Conductor)
add_subdirectory(REST)
add_subdirectory(Worker)
