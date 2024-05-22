# This code is shared with arangosh
add_library(arangoimport_utils OBJECT
  AutoTuneThread.cpp
  ImportHelper.cpp
  SenderThread.cpp)
target_include_directories(arangoimport_utils PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)
target_link_libraries(arangoimport_utils arango)

add_executable(${BIN_ARANGOIMPORT}
  ${ProductVersionFiles_arangoimport}
  ImportFeature.cpp
  arangoimport.cpp
)
target_include_directories(${BIN_ARANGOIMPORT} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOIMPORT}
  arango
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  arangoimport_utils
  clienttools_utils
)

install(
  TARGETS ${BIN_ARANGOIMPORT}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGOIMPORT})

add_dependencies(${BIN_ARANGOIMPORT} zlibstatic)

# use ICU 64 for the client tools
add_dependencies(${BIN_ARANGOIMPORT} iculegacy_build)

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGOIMPORT} jemalloc_build)
endif ()

add_dependencies(${BIN_ARANGOIMPORT} man)

install_command_alias(${BIN_ARANGOIMPORT}
  ${CMAKE_INSTALL_BINDIR}
  arangoimp
)

