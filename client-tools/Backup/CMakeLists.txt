if (USE_ENTERPRISE)

  if (MSVC AND NOT(SKIP_PACKAGING))
    set(ARANGO_BACKUP_FRIENDLY_STRING  "arangobackup - hot backup tool")
    generate_product_version(ProductVersionFiles_arangobackup
      NAME arangobackup
      FILE_DESCRIPTION ${ARANGO_BACKUP_FRIENDLY_STRING}
      ICON ${ARANGO_ICON}
      COMPANY_NAME ${ARANGODB_PACKAGE_VENDOR}
      VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
      VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
      VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
      VERSION_REVISION ${BUILD_ID}
    )
  endif ()

  add_executable(${BIN_ARANGOBACKUP}
    ${ProductVersionFiles_arangobackup}
    ${CMAKE_SOURCE_DIR}/cmake/activeCodePage.manifest
    BackupFeature.cpp
    arangobackup.cpp
  )
  target_include_directories(${BIN_ARANGOBACKUP} PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)
  target_include_directories(${BIN_ARANGOBACKUP} PRIVATE ${PROJECT_SOURCE_DIR}/arangod)

  target_link_libraries(${BIN_ARANGOBACKUP}
    arango
    ${MSVC_LIBS}
    ${SYSTEM_LIBRARIES}
    boost_system
    boost_boost
    arango_shell
    clienttools_utils
    fuerte
  )

  install(
    TARGETS ${BIN_ARANGOBACKUP}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  install_config(${BIN_ARANGOBACKUP})

  add_dependencies(${BIN_ARANGOBACKUP} zlibstatic)

  if (NOT USE_PRECOMPILED_V8)
    add_dependencies(${BIN_ARANGOBACKUP} v8_build) # v8_build includes ICU build
  endif ()

  if (USE_JEMALLOC)
    add_dependencies(${BIN_ARANGOBACKUP} jemalloc_build)
  endif ()

  if (UNIX) 
    add_dependencies(${BIN_ARANGOBACKUP} man)
  endif ()
endif () # USE_ENTERPRISE

