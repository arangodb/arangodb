add_executable(${BIN_ARANGODUMP}
  ${ProductVersionFiles_arangodump}
  DumpFeature.cpp
  arangodump.cpp
)
target_include_directories(${BIN_ARANGODUMP} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGODUMP}
  arango
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  clienttools_utils
)

install(
  TARGETS ${BIN_ARANGODUMP}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGODUMP})

add_dependencies(${BIN_ARANGODUMP} zlibstatic)

# use ICU 64 for the client tools
add_dependencies(${BIN_ARANGODUMP} iculegacy_build)

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGODUMP} jemalloc_build)
endif ()

add_dependencies(${BIN_ARANGODUMP} man)
