if (MSVC AND NOT(SKIP_PACKAGING))
  set(ARANGO_IMPORT_FRIENDLY_STRING  "arangoimport - data importer")
  generate_product_version(ProductVersionFiles_arangoimport
    NAME arangoimport
    FILE_DESCRIPTION ${ARANGO_IMPORT_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    COMPANY_NAME ${ARANGODB_PACKAGE_VENDOR}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

# This code is shared with arangosh
add_library(arangoimport_utils OBJECT
  AutoTuneThread.cpp
  ImportHelper.cpp
  SenderThread.cpp)
target_include_directories(arangoimport_utils PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)
target_link_libraries(arangoimport_utils arango)

add_executable(${BIN_ARANGOIMPORT}
  ${ProductVersionFiles_arangoimport}
  ${CMAKE_SOURCE_DIR}/cmake/activeCodePage.manifest
  ImportFeature.cpp
  arangoimport.cpp
)
target_include_directories(${BIN_ARANGOIMPORT} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOIMPORT}
  arango
  ${MSVC_LIBS}
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

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(${BIN_ARANGOIMPORT} v8_build) # v8_build includes ICU
endif ()

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGOIMPORT} jemalloc_build)
endif ()

if (UNIX) 
  add_dependencies(${BIN_ARANGOIMPORT} man)
endif ()

install_command_alias(${BIN_ARANGOIMPORT}
  ${CMAKE_INSTALL_BINDIR}
  arangoimp
)

