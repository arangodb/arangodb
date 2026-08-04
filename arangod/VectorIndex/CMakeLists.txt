add_library(arango_vector_index STATIC
  VectorIndexBuildManager.cpp
  VectorIndexFeature.cpp
  VectorIndexOptionsProvider.cpp
  VectorIndexTrainingSampler.cpp
  VectorIndexUtils.cpp)

target_link_libraries(arango_vector_index
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_vector_index PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")