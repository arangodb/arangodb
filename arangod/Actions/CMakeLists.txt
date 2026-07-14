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

add_executable(probe_actions
  ${PROJECT_SOURCE_DIR}/tests/Probe/probe_main.cpp)

target_link_options(probe_actions PRIVATE
  "-Wl,--whole-archive"
  "$<TARGET_FILE:arango_actions>"
  "-Wl,--no-whole-archive")

target_link_libraries(probe_actions PRIVATE arango_actions)
