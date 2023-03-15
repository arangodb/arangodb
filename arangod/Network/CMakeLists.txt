add_library(arango_network STATIC
  ClusterUtils.cpp
  ConnectionPool.cpp
  Methods.cpp
  NetworkFeature.cpp
  Utils.cpp)

target_link_libraries(arango_network
  boost_boost
  fuerte
  llhttp
  nghttp2
  arango::validation
  arango_replication2
  immer)

target_include_directories(arango_network PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
