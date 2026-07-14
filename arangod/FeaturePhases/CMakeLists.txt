add_library(arango_feature_phases STATIC
  AgencyFeaturePhase.cpp
  AqlFeaturePhase.cpp
  BasicFeaturePhaseServer.cpp
  ClusterFeaturePhase.cpp
  DatabaseFeaturePhase.cpp
  FinalFeaturePhase.cpp
  ServerFeaturePhase.cpp)

if(USE_V8)
  target_sources(arango_feature_phases PRIVATE
    FoxxFeaturePhase.cpp
    V8FeaturePhase.cpp)
endif()

target_link_libraries(arango_feature_phases
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_feature_phases PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_executable(probe_feature_phases
  ${PROJECT_SOURCE_DIR}/tests/Probe/probe_main.cpp)

target_link_options(probe_feature_phases PRIVATE
  "-Wl,--whole-archive"
  "$<TARGET_FILE:arango_feature_phases>"
  "-Wl,--no-whole-archive")

target_link_libraries(probe_feature_phases PRIVATE arango_feature_phases)
