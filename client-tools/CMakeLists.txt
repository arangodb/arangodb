# -*- mode: CMAKE; -*-

if (USE_V8)
  include(InstallArangoDBJSClient)
endif ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

add_compile_warnings_flags()

if (USE_GOOGLE_TESTS)
  add_definitions("-DTEST_VIRTUAL=virtual")
else()
  add_definitions("-DTEST_VIRTUAL=")
endif()

add_subdirectory(Benchmark)
add_subdirectory(Backup)
add_subdirectory(Dump)
add_subdirectory(Export)
add_subdirectory(Import)
add_subdirectory(Restore)
add_subdirectory(Shell)
add_subdirectory(Utils)
add_subdirectory(VPack)

add_custom_target(client-tools
    DEPENDS arangodump
            arangoexport
            arangoimport
            arangorestore
            arangobench
            arangovpack)

if (USE_V8)
  add_dependencies(client-tools arangosh)
endif ()

if (USE_ENTERPRISE)
  add_dependencies(client-tools arangobackup)
endif ()
