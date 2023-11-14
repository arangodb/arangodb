if (MSVC AND NOT(SKIP_PACKAGING))
  generate_product_version(ProductVersionFiles_arangobench
    NAME arangobench
    FILE_DESCRIPTION ${ARANGOBENCH_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOBENCH}
  ${ProductVersionFiles_arangobench}
  arangobench.cpp
  BenchFeature.cpp
  BenchmarkOperation.cpp
  testcases/helpers.cpp
  ${CMAKE_SOURCE_DIR}/cmake/activeCodePage.manifest
)
target_include_directories(${BIN_ARANGOBENCH} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)

target_link_libraries(${BIN_ARANGOBENCH}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  clienttools_utils
)

install(
  TARGETS ${BIN_ARANGOBENCH}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangobench)

add_dependencies(arangobench zlibstatic)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangobench v8_build) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangobench jemalloc_build)
endif ()

