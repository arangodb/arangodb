add_library(arango_network STATIC
  ClusterUtils.cpp
  ConnectionPool.cpp
  Methods.cpp
  NetworkFeature.cpp
  Utils.cpp)

add_library(arango_curl STATIC curl-stuff.cpp)

target_link_libraries(arango_curl
        PUBLIC
        arango_lightweight
        fuerte # TODO remove - only used for error code translation
        PRIVATE
        libcurl)
target_include_directories(arango_curl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# make sure curl is build with deprecated stuff removed.
# enable non-compatibility mode
target_compile_definitions(arango_curl PRIVATE CURL_NO_OLDIES)


target_link_libraries(arango_network PUBLIC
  boost_boost
  fuerte
  llhttp
  nghttp2
  arango::validation
  arango_replication2
  immer)

target_link_libraries(arango_network PRIVATE
        arango_curl)

target_include_directories(arango_network PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
