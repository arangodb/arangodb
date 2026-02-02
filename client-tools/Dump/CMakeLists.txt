add_executable(${BIN_ARANGODUMP}
  ${ProductVersionFiles_arangodump}
  DumpFeature.cpp
  arangodump.cpp
)
target_include_directories(${BIN_ARANGODUMP} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGODUMP}
  ${SYSTEM_LIBRARIES}
  arango_basic_maskings
  boost_system
  boost_boost
  arango_shell
  clienttools_utils
)

if (USE_JEMALLOC)
  target_link_libraries(${BIN_ARANGODUMP} jemalloc)
endif ()

install(
  TARGETS ${BIN_ARANGODUMP}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGODUMP})

add_dependencies(${BIN_ARANGODUMP} zlibstatic)

# use ICU 64 for the client tools
add_dependencies(${BIN_ARANGODUMP} iculegacy_build)

add_dependencies(${BIN_ARANGODUMP} man)
