# -*- mode: CMAKE; -*-

include_directories(.)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

################################################################################
## shell common
################################################################################

set(LIB_ARANGO_SHELL_SOURCES
  FeaturePhases/BasicFeaturePhaseClient.cpp
  FeaturePhases/V8ShellFeaturePhase.cpp
  Shell/ClientFeature.cpp
  Shell/ConsoleFeature.cpp  
  Utils/ClientManager.cpp
  Utils/ManagedDirectory.cpp
  V8Client/ArangoClientHelper.cpp
)

add_library(arango_shell STATIC
  ${LIB_ARANGO_SHELL_SOURCES}
)

target_link_libraries(arango_shell
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
)

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
)
target_include_directories(${BIN_ARANGOBENCH} PRIVATE ${PROJECT_SOURCE_DIR}/arangosh)

target_link_libraries(${BIN_ARANGOBENCH}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
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
  add_dependencies(arangobench jemalloc_build)
endif ()

if (USE_FAIL_ON_WARNINGS)
 if (MSVC)
   target_compile_options(arangobench PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
 else ()
   target_compile_options(arangobench PRIVATE -Werror -Wno-error=deprecated-declarations)
 endif ()
endif ()

################################################################################
## arangobackup
################################################################################

if (USE_ENTERPRISE)

if (MSVC AND NOT(SKIP_PACKAGING))
  generate_product_version(ProductVersionFiles_arangobackup
    NAME arangobackup
    FILE_DESCRIPTION ${ARANGO_BACKUP_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOBACKUP}
  ${ProductVersionFiles_arangobackup}
  Backup/BackupFeature.cpp
  Backup/arangobackup.cpp
  ${ADDITIONAL_BIN_ARANGOBACKUP_SOURCES}
)
target_include_directories(${BIN_ARANGOBACKUP} PRIVATE ${PROJECT_SOURCE_DIR}/arangosh)

target_link_libraries(${BIN_ARANGOBACKUP}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
)

install(
  TARGETS ${BIN_ARANGOBACKUP}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangobackup)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangobackup zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangobackup zlibstatic)
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangobackup jemalloc_build)
endif ()

if (USE_FAIL_ON_WARNINGS)
 if (MSVC)
   target_compile_options(arangobackup PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
 else ()
   target_compile_options(arangobackup PRIVATE -Werror -Wno-error=deprecated-declarations)
 endif ()
endif ()

endif () # USE_ENTERPRISE

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
  ${ADDITIONAL_BIN_ARANGODUMP_SOURCES}
)
target_include_directories(${BIN_ARANGODUMP} PRIVATE ${PROJECT_SOURCE_DIR}/arangosh)

target_link_libraries(${BIN_ARANGODUMP}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
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
  add_dependencies(arangodump jemalloc_build)
endif ()

if (USE_FAIL_ON_WARNINGS)
 if (MSVC)
   target_compile_options(arangodump PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
 else ()
   target_compile_options(arangodump PRIVATE -Werror -Wno-error=deprecated-declarations)
 endif ()
endif ()

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
)
target_include_directories(${BIN_ARANGOEXPORT} PRIVATE ${PROJECT_SOURCE_DIR}/arangosh)

target_link_libraries(${BIN_ARANGOEXPORT}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
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
  add_dependencies(arangoexport jemalloc_build)
endif ()

if (USE_FAIL_ON_WARNINGS)
 if (MSVC)
   target_compile_options(arangoexport PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
 else ()
   target_compile_options(arangoexport PRIVATE -Werror -Wno-error=deprecated-declarations)
 endif ()
endif ()

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
)
target_include_directories(${BIN_ARANGOIMPORT} PRIVATE ${PROJECT_SOURCE_DIR}/arangosh)

target_link_libraries(${BIN_ARANGOIMPORT}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
)

install(
  TARGETS arangoimport
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangoimport)

add_dependencies(arangoimport fuerte)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangoimport zlibstatic v8_build) # v8_build includes ICU
                                                     # build
else ()
  add_dependencies(arangoimport zlibstatic) # v8_build includes ICU build
endif ()

if (USE_JEMALLOC)
  add_dependencies(arangoimport jemalloc_build)
endif ()

install_command_alias(arangoimport
  ${CMAKE_INSTALL_BINDIR}
  arangoimp
)

if (USE_FAIL_ON_WARNINGS)
 if (MSVC)
   target_compile_options(arangoimport PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
 else ()
   target_compile_options(arangoimport PRIVATE -Werror -Wno-error=deprecated-declarations)
 endif ()
endif ()

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
  ${ADDITIONAL_BIN_ARANGORESTORE_SOURCES}
)
target_include_directories(${BIN_ARANGORESTORE} PRIVATE ${PROJECT_SOURCE_DIR}/arangosh)

target_link_libraries(${BIN_ARANGORESTORE}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
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
  add_dependencies(arangorestore jemalloc_build)
endif ()

if (USE_FAIL_ON_WARNINGS)
 if (MSVC)
   target_compile_options(arangorestore PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
 else ()
   target_compile_options(arangorestore PRIVATE -Werror -Wno-error=deprecated-declarations)
 endif ()
endif ()

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
  Shell/ShellFeature.cpp
  Shell/V8ClientConnection.cpp
  Shell/V8ShellFeature.cpp
  Shell/arangosh.cpp
)
target_include_directories(${BIN_ARANGOSH} PRIVATE ${PROJECT_SOURCE_DIR}/arangosh)

target_link_libraries(${BIN_ARANGOSH}
  arango_v8_deadline
  arango_v8
  arango
  ${LINENOISE_LIBS}
  ${V8_LIBS}
  fuerte
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
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
  add_dependencies(arangosh jemalloc_build)
endif ()

if (USE_FAIL_ON_WARNINGS)
 if (MSVC)
   target_compile_options(arangosh PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
 else ()
   target_compile_options(arangosh PRIVATE -Werror -Wno-error=deprecated-declarations)
 endif ()
endif ()

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
  VPack/VPackFeature.cpp
  VPack/arangovpack.cpp
)
target_include_directories(${BIN_ARANGOVPACK} PRIVATE ${PROJECT_SOURCE_DIR}/arangosh)

target_link_libraries(${BIN_ARANGOVPACK}
  arango
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
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
  add_dependencies(arangovpack jemalloc_build)
endif ()

if (USE_FAIL_ON_WARNINGS)
 if (MSVC)
   target_compile_options(arangovpack PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
 else ()
   target_compile_options(arangovpack PRIVATE -Werror -Wno-error=deprecated-declarations)
 endif ()
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
