add_library(clienttools_utils STATIC
  ClientManager.cpp
  ManagedDirectory.cpp
)

target_link_libraries(clienttools_utils
  ${SYSTEM_LIBRARIES}
  arango_basic_errors
  arango_basic_utils
  arango_scheduler
  boost_system
  boost_boost
  fuerte
  v8_interface
)
target_include_directories(clienttools_utils PRIVATE ${CMAKE_SOURCE_DIR}/client-tools)

