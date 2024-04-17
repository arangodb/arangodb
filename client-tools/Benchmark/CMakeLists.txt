add_executable(${BIN_ARANGOBENCH}
  ${ProductVersionFiles_arangobench}
  arangobench.cpp
  BenchFeature.cpp
  BenchmarkOperation.cpp
  testcases/helpers.cpp
)
target_include_directories(${BIN_ARANGOBENCH} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOBENCH}
  arango
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  clienttools_utils
)

install(
  TARGETS ${BIN_ARANGOBENCH}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGOBENCH})

add_dependencies(${BIN_ARANGOBENCH} zlibstatic)

# use ICU 64 for the client tools
add_dependencies(${BIN_ARANGOBENCH} iculegacy_build)

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGOBENCH} jemalloc_build)
endif ()

add_dependencies(${BIN_ARANGOBENCH} man)
