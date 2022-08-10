add_library(arango_pregel STATIC
  AggregatorHandler.cpp
  AlgoRegistry.cpp
  Algos/AsyncSCC.cpp
  Algos/ConnectedComponents.cpp
  Algos/DMID/DMID.cpp
  Algos/EffectiveCloseness/EffectiveCloseness.cpp
  Algos/EffectiveCloseness/HLLCounter.cpp
  Algos/HITS.cpp
  Algos/LabelPropagation.cpp
  Algos/LineRank.cpp
  Algos/PageRank.cpp
  Algos/RecoveringPageRank.cpp
  Algos/SCC.cpp
  Algos/SLPA.cpp
  Algos/SSSP.cpp
  Algos/ShortestPath.cpp
  Algos/WCC.cpp
  Conductor.cpp
  GraphStore.cpp
  IncomingCache.cpp
  IndexHelpers.cpp
  OutgoingCache.cpp
  PregelFeature.cpp
  PregelMetrics.cpp
  Recovery.cpp
  Reports.cpp
  Reports.h
  Utils.cpp
  Worker.cpp
  WorkerConfig.cpp
  Conductor/InitialState.cpp
  Conductor/LoadingState.cpp
  Conductor/ComputingState.cpp
  Conductor/StoringState.cpp
  Conductor/CanceledState.cpp
  Conductor/DoneState.cpp
  Conductor/InErrorState.cpp
  Conductor/RecoveringState.cpp
  Conductor/FatalErrorState.cpp
  Algos/AIR/AIR.cpp
  Algos/AIR/AbstractAccumulator.cpp
  Algos/AIR/AccumulatorOptions.cpp
  Algos/AIR/Accumulators.cpp
  Algos/AIR/EdgeData.cpp
  Algos/AIR/GraphFormat.cpp
  Algos/AIR/MasterContext.cpp
  Algos/AIR/MessageData.cpp
  Algos/AIR/MessageFormat.cpp
  Algos/AIR/VertexComputation.cpp
  Algos/AIR/VertexData.cpp
  Algos/AIR/WorkerContext.cpp
  ../RestHandler/RestControlPregelHandler.cpp
  ../RestHandler/RestPregelHandler.cpp)

target_link_libraries(arango_pregel
	arango
  arango_agency
  arango_greenspun
  boost_boost
  boost_system)

target_include_directories(arango_pregel PRIVATE ${PROJECT_SOURCE_DIR}/arangod)
target_include_directories(arango_pregel PRIVATE "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

if (USE_FAIL_ON_WARNINGS)
  if (MSVC)
    target_compile_options(arango_pregel PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # clang-cl.exe
      # There's a warning about /MP being unused if compiling only a single file
      target_compile_options(arango_pregel PRIVATE -Wno-unused-command-line-argument)
    endif()
  else ()
    target_compile_options(arango_pregel PRIVATE -Werror -Wno-error=deprecated-declarations)
  endif ()
endif ()
