add_library(arango_auth STATIC
  Common.cpp
  TokenCache.cpp
  User.cpp
  UserManagerImpl.cpp)

target_link_libraries(arango_auth
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_auth PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_executable(probe_auth
  ${PROJECT_SOURCE_DIR}/tests/Probe/probe_main.cpp)

target_link_options(probe_auth PRIVATE
  "-Wl,--whole-archive"
  "$<TARGET_FILE:arango_auth>"
  "-Wl,--no-whole-archive")

target_link_libraries(probe_auth PRIVATE arango_auth)
