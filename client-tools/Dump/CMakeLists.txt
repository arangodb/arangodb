if (MSVC AND NOT(SKIP_PACKAGING))
  set(ARANGO_DUMP_FRIENDLY_STRING    "arangodump - data and configuration dumping tool")
  generate_product_version(ProductVersionFiles_arangodump
    NAME arangodump
    FILE_DESCRIPTION ${ARANGO_DUMP_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    COMPANY_NAME ${ARANGODB_PACKAGE_VENDOR}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGODUMP}
  ${ProductVersionFiles_arangodump}
  ${CMAKE_SOURCE_DIR}/cmake/activeCodePage.manifest
  DumpFeature.cpp
  arangodump.cpp
)
target_include_directories(${BIN_ARANGODUMP} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGODUMP}
  arango
  ${MSVC_LIBS}
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

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(${BIN_ARANGODUMP} v8_build) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGODUMP} jemalloc_build)
endif ()

if (UNIX) 
  add_dependencies(${BIN_ARANGODUMP} man)
endif ()
