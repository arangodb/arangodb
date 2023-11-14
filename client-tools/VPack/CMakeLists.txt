if (MSVC AND NOT(SKIP_PACKAGING))
  generate_product_version(ProductVersionFiles_arangovpack
    NAME arangovpack
    FILE_DESCRIPTION ${ARANGO_VPACK_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOVPACK}
  ${ProductVersionFiles_arangovpack}
  VPackFeature.cpp
  arangovpack.cpp
)
target_include_directories(${BIN_ARANGOVPACK} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOVPACK}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  clienttools_utils
)

install(
  TARGETS ${BIN_ARANGOVPACK}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangovpack)

add_dependencies(arangovpack zlibstatic)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangovpack v8_build)
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangovpack jemalloc_build)
endif ()
