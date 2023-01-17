add_library(arango_pregel STATIC
  AggregatorHandler.cpp
  AlgoRegistry.cpp
  Conductor.cpp
  ExecutionNumber.cpp
  GraphStore.cpp
  IncomingCache.cpp
  IndexHelpers.cpp
  OutgoingCache.cpp
  PregelFeature.cpp
  PregelMetrics.cpp
  Utils.cpp
  Worker.cpp
  WorkerConfig.cpp)

target_link_libraries(arango_pregel
  arango
  arango_agency
  boost_boost
  boost_system)

target_include_directories(arango_pregel PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_subdirectory(Algos)
add_subdirectory(REST)
