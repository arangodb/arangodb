# -*- mode: CMAKE; -*-

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)

# options
option(BuildBench "Build bench performance test suite" OFF)
message(STATUS "VelocyPack building bench performance test suite: ${BuildBench}")

option(BuildTools "Build support programs and tools" ON)
message(STATUS "VelocyPack building support programs and tools: ${BuildTools}")

if(BuildTools)
  # build json-to-vpack.cpp
  add_executable("json-to-vpack" json-to-vpack.cpp)
  target_link_libraries("json-to-vpack" velocypack)
  install(TARGETS "json-to-vpack" DESTINATION bin)

  # build vpack-to-json.cpp
  add_executable("vpack-to-json" vpack-to-json.cpp)
  target_link_libraries("vpack-to-json" velocypack)
  install(TARGETS "vpack-to-json" DESTINATION bin)
  
  # build vpack-validate.cpp
  add_executable("vpack-validate" vpack-validate.cpp)
  target_link_libraries("vpack-validate" velocypack)
  install(TARGETS "vpack-validate" DESTINATION bin)

  # build fuzzer.cpp
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  add_executable("fuzzer" fuzzer.cpp)
  target_link_libraries("fuzzer" PRIVATE velocypack Threads::Threads)
  install(TARGETS "fuzzer" DESTINATION bin)
endif()

# build bench.cpp
if(BuildBench)
  if(NOT IS_DIRECTORY "${PROJECT_SOURCE_DIR}/rapidjson")
    message(FATAL_ERROR "rapidjson library not found in subdirectory 'rapidjson'. Please run ./download-rapidjson.sh")
  endif()
  if(NOT IS_DIRECTORY "${PROJECT_SOURCE_DIR}/simdjson")
    message(FATAL_ERROR "simdjson library not found in subdirectory 'simdjson'. Please run ./download-simdjson.sh")
  endif()

  add_library(simdjson STATIC IMPORTED)
  set_target_properties(simdjson PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/simdjson/build/libsimdjson.a)

  add_executable(bench bench.cpp)
  target_link_libraries(bench velocypack)
  target_link_libraries(bench simdjson)
  target_include_directories(bench PRIVATE ${PROJECT_SOURCE_DIR}/rapidjson/include)
  target_include_directories(bench PRIVATE ${PROJECT_SOURCE_DIR}/simdjson/include)

  if(EnableSSE)
      target_compile_definitions(bench PRIVATE RAPIDJSON_SSE42)
  endif()
endif()

