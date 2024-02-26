add_library(clienttools_utils STATIC
  ClientManager.cpp
  ManagedDirectory.cpp
)

target_link_libraries(clienttools_utils
  arango
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  fuerte
  v8_interface
)
target_include_directories(clienttools_utils PRIVATE ${CMAKE_SOURCE_DIR}/client-tools)

