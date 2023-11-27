if (MSVC AND NOT(SKIP_PACKAGING))
  set(ARANGO_EXPORT_FRIENDLY_STRING  "arangoexport - data exporter")
  generate_product_version(ProductVersionFiles_arangoexport
    NAME arangoexport
    FILE_DESCRIPTION ${ARANGO_EXPORT_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    COMPANY_NAME ${ARANGODB_PACKAGE_VENDOR}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOEXPORT}
  ${ProductVersionFiles_arangoexport}
  ${CMAKE_SOURCE_DIR}/cmake/activeCodePage.manifest
  ExportFeature.cpp
  arangoexport.cpp
)
target_include_directories(${BIN_ARANGOEXPORT} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOEXPORT}
  arango
  ${MSVC_LIBS}
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

if (UNIX) 
  add_dependencies(${BIN_ARANGOEXPORT} man)
endif ()
