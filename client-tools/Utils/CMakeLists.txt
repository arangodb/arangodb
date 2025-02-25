add_library(clienttools_utils STATIC
  ClientManager.cpp
  ManagedDirectory.cpp
)

target_link_libraries(clienttools_utils
  ${SYSTEM_LIBRARIES}
  arango_basic_errors
  arango_static_strings
  arango_basic_utils
  arango_basic_features
  arango_basic_logger
  arango_basic_http_client
  arango_scheduler
  boost_system
  boost_boost
  fuerte
  v8_interface
)
target_include_directories(clienttools_utils PRIVATE ${CMAKE_SOURCE_DIR}/client-tools)

add_dependencies(clienttools_utils v8_build)

