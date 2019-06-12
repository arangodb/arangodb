# -*- mode: CMAKE; -*-

include_directories(.)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

################################################################################
## arangobench
################################################################################

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
  Benchmark/BenchFeature.cpp
  Benchmark/arangobench.cpp
  Shell/ClientFeature.cpp
  Shell/ConsoleFeature.cpp
)

target_link_libraries(${BIN_ARANGOBENCH}
  ${LIB_ARANGO}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
)

install(
  TARGETS ${BIN_ARANGOBENCH}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangobench)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangobench zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangobench zlibstatic)
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangobench jemalloc)
endif ()

if(MSVC)
  target_compile_options(arangobench PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

################################################################################
## arangodump
################################################################################

if (MSVC AND NOT(SKIP_PACKAGING))
  generate_product_version(ProductVersionFiles_arangodump
    NAME arangodump
    FILE_DESCRIPTION ${ARANGO_DUMP_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGODUMP}
  ${ProductVersionFiles_arangodump}
  Dump/DumpFeature.cpp
  Dump/arangodump.cpp
  Shell/ClientFeature.cpp
  Shell/ConsoleFeature.cpp
  Utils/ClientManager.cpp
  Utils/ManagedDirectory.cpp
  ${ADDITIONAL_BIN_ARANGODUMP_SOURCES}
)

target_link_libraries(${BIN_ARANGODUMP}
  ${LIB_ARANGO}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
)

install(
  TARGETS ${BIN_ARANGODUMP}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangodump)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangodump zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangodump zlibstatic)
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangodump jemalloc)
endif ()

if(MSVC)
  target_compile_options(arangobench PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

################################################################################
## arangoexport
################################################################################

if (MSVC AND NOT(SKIP_PACKAGING))
  generate_product_version(ProductVersionFiles_arangoexport
    NAME arangoexport
    FILE_DESCRIPTION ${ARANGO_EXPORT_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOEXPORT}
  ${ProductVersionFiles_arangoexport}
  Export/ExportFeature.cpp
  Export/arangoexport.cpp
  Shell/ClientFeature.cpp
  Shell/ConsoleFeature.cpp
  V8Client/ArangoClientHelper.cpp
)

target_link_libraries(${BIN_ARANGOEXPORT}
  ${LIB_ARANGO}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
)

install(
  TARGETS ${BIN_ARANGOEXPORT}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangoexport)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangoexport zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangoexport zlibstatic) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangoexport jemalloc)
endif ()

if(MSVC)
  target_compile_options(arangoexport PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

################################################################################
## arangoimport
################################################################################

if (MSVC AND NOT(SKIP_PACKAGING))
  generate_product_version(ProductVersionFiles_arangoimport
    NAME arangoimport
    FILE_DESCRIPTION ${ARANGO_IMPORT_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOIMPORT}
  ${ProductVersionFiles_arangoimport}
  Import/AutoTuneThread.cpp
  Import/ImportFeature.cpp
  Import/ImportHelper.cpp
  Import/SenderThread.cpp
  Import/arangoimport.cpp
  Shell/ClientFeature.cpp
  Shell/ConsoleFeature.cpp
  V8Client/ArangoClientHelper.cpp
)

target_link_libraries(${BIN_ARANGOIMPORT}
  ${LIB_ARANGO}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
)

install(
  TARGETS ${BIN_ARANGOIMPORT}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangoimport)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangoimport zlibstatic v8_build) # v8_build includes ICU
                                                     # build
else ()
  add_dependencies(arangoimport zlibstatic) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangoimport jemalloc)
endif ()

install_command_alias(${BIN_ARANGOIMPORT}
  ${CMAKE_INSTALL_BINDIR}
  arangoimport
)

if(MSVC)
  target_compile_options(arangoimp PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()
################################################################################
## arangorestore
################################################################################

if (MSVC AND NOT(SKIP_PACKAGING))
  generate_product_version(ProductVersionFiles_arangorestore
    NAME arangorestore
    FILE_DESCRIPTION ${ARANGO_RESTORE_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGORESTORE}
  ${ProductVersionFiles_arangorestore}
  Restore/RestoreFeature.cpp
  Restore/arangorestore.cpp
  Shell/ClientFeature.cpp
  Shell/ConsoleFeature.cpp
  Utils/ClientManager.cpp
  Utils/ManagedDirectory.cpp
  ${ADDITIONAL_BIN_ARANGORESTORE_SOURCES}
)

target_link_libraries(${BIN_ARANGORESTORE}
  ${LIB_ARANGO}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
)

install(
  TARGETS ${BIN_ARANGORESTORE}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangorestore)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangorestore zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangorestore zlibstatic) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangorestore jemalloc)
endif ()

if(MSVC)
  target_compile_options(arangorestore PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()
################################################################################
## arangosh
################################################################################

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

add_executable(${BIN_ARANGOSH}
  ${ProductVersionFiles_arangosh}
  Import/AutoTuneThread.cpp
  Import/ImportHelper.cpp
  Import/SenderThread.cpp
  Shell/ClientFeature.cpp
  Shell/ConsoleFeature.cpp
  Shell/ShellFeature.cpp
  Shell/V8ClientConnection.cpp
  Shell/V8ShellFeature.cpp
  Shell/arangosh.cpp
  V8Client/ArangoClientHelper.cpp
)

target_link_libraries(${BIN_ARANGOSH}
  ${LIB_ARANGO_V8}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${V8_LIBS}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
)

install(
  TARGETS ${BIN_ARANGOSH}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangosh)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangosh zlibstatic v8_build)
else ()
  add_dependencies(arangosh zlibstatic)
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangosh jemalloc)
endif ()

if(MSVC)
  target_compile_options(arangosh PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()
################################################################################
## arangovpack
################################################################################

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
  Shell/ClientFeature.cpp
  Shell/ConsoleFeature.cpp
  VPack/VPackFeature.cpp
  VPack/arangovpack.cpp
)

target_link_libraries(${BIN_ARANGOVPACK}
  ${LIB_ARANGO}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
)

install(
  TARGETS ${BIN_ARANGOVPACK}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangovpack)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangovpack zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangovpack zlibstatic) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangovpack jemalloc)
endif ()

if(MSVC)
  target_compile_options(arangovpack PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()
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
