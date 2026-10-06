add_library(arango_actions STATIC
  ActionFeature.cpp
  ActionOptionsProvider.cpp
  RestActionHandler.cpp
  actions.cpp)

target_link_libraries(arango_actions
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_actions PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")