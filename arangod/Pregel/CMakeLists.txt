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
  Recovery.cpp
  Reports.cpp
  Reports.h
  Utils.cpp
  Worker.cpp
  WorkerConfig.cpp)

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

add_subdirectory(REST)
add_subdirectory(Algos)
add_subdirectory(Conductor)
