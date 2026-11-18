add_library(arango_feature_phases STATIC
  AgencyFeaturePhase.cpp
  AqlFeaturePhase.cpp
  BasicFeaturePhaseServer.cpp
  ClusterFeaturePhase.cpp
  DatabaseFeaturePhase.cpp
  FinalFeaturePhase.cpp
  ServerFeaturePhase.cpp)

target_link_libraries(arango_feature_phases
  PUBLIC
    arango_basic_utils
    arango_rest_server
    arango_scheduler
    arango_vector_index
    )

target_include_directories(arango_feature_phases PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")