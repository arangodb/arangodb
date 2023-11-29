if (MSVC AND NOT(SKIP_PACKAGING))
  set(ARANGO_BENCH_FRIENDLY_STRING   "arangobench - stress test tool")
  generate_product_version(ProductVersionFiles_arangobench
    NAME arangobench
    FILE_DESCRIPTION ${ARANGO_BENCH_FRIENDLY_STRING}
    ICON ${ARANGO_ICON} 
    COMPANY_NAME ${ARANGODB_PACKAGE_VENDOR}
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

install_config(${BIN_ARANGOBENCH})

add_dependencies(${BIN_ARANGOBENCH} zlibstatic)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(${BIN_ARANGOBENCH} v8_build) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(${BIN_ARANGOBENCH} jemalloc_build)
endif ()

if (UNIX) 
  add_dependencies(${BIN_ARANGOBENCH} man)
endif ()
