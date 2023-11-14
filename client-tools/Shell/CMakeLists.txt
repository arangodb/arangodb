if (MSVC AND NOT(SKIP_PACKAGING))
  generate_product_version(ProductVersionFiles_arangosh
    NAME arangosh
    FILE_DESCRIPTION ${ARANGOSH_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_library(arango_shell STATIC
  ClientFeature.cpp
  ShellConsoleFeature.cpp
  ${CMAKE_SOURCE_DIR}/cmake/activeCodePage.manifest
)

target_link_libraries(arango_shell
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  fuerte
  v8_interface
)
target_include_directories(arango_shell PRIVATE ${CMAKE_SOURCE_DIR}/client-tools)

add_executable(${BIN_ARANGOSH}
  ${ProductVersionFiles_arangosh}
  ${CMAKE_SOURCE_DIR}/cmake/activeCodePage.manifest

  ShellFeature.cpp
  TelemetricsHandler.cpp
  V8ClientConnection.cpp
  V8ShellFeature.cpp
  ProcessMonitoringFeature.cpp
  arangosh.cpp
  v8-deadline.cpp
)

if (USE_FAILURE_TESTS)
    target_sources(${BIN_ARANGOSH} PRIVATE RequestFuzzer.cpp)
endif()

target_include_directories(${BIN_ARANGOSH} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOSH}
  arango
  arango_v8
  ${V8_LIBS}
  fuerte
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  arangoimport_utils
  clienttools_utils
)

install(FILES ${ICU_DT}
  DESTINATION "${INSTALL_ICU_DT_DEST}"
  RENAME ${ICU_DT_DEST})

install(
  TARGETS ${BIN_ARANGOSH}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangosh)

add_dependencies(arangosh zlibstatic)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangosh v8_build)
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangosh jemalloc_build)
endif ()

################################################################################
## foxx-manager
################################################################################
install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_BINDIR}
  foxx-manager)

install_config(foxx-manager)

################################################################################
## arangoinspect
################################################################################
install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_BINDIR}
  arangoinspect)

install_config(arangoinspect)


