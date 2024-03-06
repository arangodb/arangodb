add_executable(${BIN_ARANGOEXPORT}
  ${ProductVersionFiles_arangoexport}
  ExportFeature.cpp
  arangoexport.cpp
)
target_include_directories(${BIN_ARANGOEXPORT} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOEXPORT}
  arango
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  clienttools_utils
)

install(
  TARGETS ${BIN_ARANGOEXPORT}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGOEXPORT})

add_dependencies(${BIN_ARANGOEXPORT} zlibstatic)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(${BIN_ARANGOEXPORT} v8_build) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGOEXPORT} jemalloc_build)
endif ()

add_dependencies(${BIN_ARANGOEXPORT} man)
