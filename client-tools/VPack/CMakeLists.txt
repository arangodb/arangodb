add_executable(${BIN_ARANGOVPACK}
  ${ProductVersionFiles_arangovpack}
  VPackFeature.cpp
  arangovpack.cpp
)
target_include_directories(${BIN_ARANGOVPACK} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOVPACK}
  arango
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  clienttools_utils
)

install(
  TARGETS ${BIN_ARANGOVPACK}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGOVPACK})

add_dependencies(${BIN_ARANGOVPACK} zlibstatic)

# use ICU 64 for the client tools
add_dependencies(${BIN_ARANGOVPACK} iculegacy_build)

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGOVPACK} jemalloc_build)
endif ()

add_dependencies(${BIN_ARANGOVPACK} man)
