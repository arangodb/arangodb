add_executable(${BIN_ARANGOEXPORT}
  ${ProductVersionFiles_arangoexport}
  ArangoExportServer.cpp
  ExportFeature.cpp
  ExportFeatureOptions.cpp
  ExportOptionsProvider.cpp
  arangoexport.cpp
)
target_include_directories(${BIN_ARANGOEXPORT} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOEXPORT}
PUBLIC
  ${SYSTEM_LIBRARIES}
  arango_shell
  clienttools_utils
PRIVATE 
  Boost::property_tree
)
if (USE_JEMALLOC)
  target_link_libraries(${BIN_ARANGOEXPORT} PUBLIC jemalloc)
endif ()


install(
  TARGETS ${BIN_ARANGOEXPORT}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGOEXPORT})

add_dependencies(${BIN_ARANGOEXPORT} zlibstatic)

# use ICU 64 for the client tools
add_dependencies(${BIN_ARANGOEXPORT} iculegacy_build)
add_dependencies(${BIN_ARANGOEXPORT} man)
