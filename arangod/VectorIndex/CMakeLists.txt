add_library(arango_vector_index STATIC
  VectorIndexBuildManager.cpp
  VectorIndexFeature.cpp
  VectorIndexOptionsProvider.cpp
  VectorIndexTrainingSampler.cpp)

target_link_libraries(arango_vector_index
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_vector_index PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_executable(probe_vector_index
  ${PROJECT_SOURCE_DIR}/tests/Probe/probe_main.cpp)

target_link_options(probe_vector_index PRIVATE
  "-Wl,--whole-archive"
  "$<TARGET_FILE:arango_vector_index>"
  "-Wl,--no-whole-archive")

target_link_libraries(probe_vector_index PRIVATE arango_vector_index)
