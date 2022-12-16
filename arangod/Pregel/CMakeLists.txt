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
  Reports.cpp
  Reports.h
  Utils.cpp
  Worker.cpp
  WorkerConfig.cpp
  ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestControlPregelHandler.cpp
  ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestPregelHandler.cpp)

target_link_libraries(arango_pregel
	arango
  arango_agency
  boost_boost
  boost_system)

target_include_directories(arango_pregel PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_subdirectory(Algos)
